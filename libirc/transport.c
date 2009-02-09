
/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2008 Jelmer Vernooij <jelmer@nl.linux.org>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "internals.h"
#include "transport.h"
#include "line.h"
#include "util.h"
#include <glib.h>
#include <sys/socket.h>
#include <ctype.h>
#include <fcntl.h>
#include <netdb.h>

static gboolean transport_send_queue(GIOChannel *c, GIOCondition cond, 
										 void *_client);

static gboolean handle_recv_status(struct irc_transport *transport, GIOStatus status, GError *error)
{
	if (status == G_IO_STATUS_EOF) {
		transport->callbacks->hangup(transport);
		return FALSE;
	}

	if (status != G_IO_STATUS_AGAIN) {
		if (error->domain == G_CONVERT_ERROR &&
			error->code == G_CONVERT_ERROR_ILLEGAL_SEQUENCE) {
			transport->callbacks->charset_error(transport, 
												error->message);
		} else {
			return transport->callbacks->error(transport, error?error->message:NULL);
		}
	}

	return TRUE;
}

static gboolean handle_transport_receive(GIOChannel *c, GIOCondition cond, 
									  void *_transport)
{
	struct irc_transport *transport = _transport;
	struct irc_line *l;

	g_assert(transport);

	if (cond & G_IO_ERR) {
		char *tmp = g_strdup_printf("Error reading from client: %s", 
						  g_io_channel_unix_get_sock_error(c));
		transport->callbacks->error(transport, tmp);
		g_free(tmp);
		return FALSE;
	}


	if (cond & G_IO_IN) {
		GError *error = NULL;
		GIOStatus status;
		gboolean ret = TRUE;
		
		while ((status = irc_recv_line(c, transport->incoming_iconv, &error, 
									   &l)) == G_IO_STATUS_NORMAL) {

			ret &= transport->callbacks->recv(transport, l);
			free_line(l);

			if (!ret)
				return FALSE;
		}

		ret &= handle_recv_status(transport, status, error);
		if (error != NULL)
			g_error_free(error);
		return ret;
	}

	if (cond & G_IO_HUP) {
		transport->callbacks->hangup(transport);
		return FALSE;
	}


	return TRUE;
}

void irc_transport_set_callbacks(struct irc_transport *transport, 
								 const struct irc_transport_callbacks *callbacks, void *userdata)
{
	transport->userdata = userdata;
	transport->callbacks = callbacks;

	transport->incoming_id = g_io_add_watch(
							transport->incoming, G_IO_IN | G_IO_HUP, 
							handle_transport_receive, transport);
}

/* GIOChannels passed into this function 
 * should preferably:
 *  - have no encoding set
 *  - work asynchronously
 *
 * @param iochannel Channel to talk over 
 */
struct irc_transport *irc_transport_new_iochannel(GIOChannel *iochannel)
{
	struct irc_transport *ret = g_new0(struct irc_transport, 1);

	ret->incoming = iochannel;
	ret->pending_lines = g_queue_new();
	ret->outgoing_iconv = ret->incoming_iconv = (GIConv)-1;
	g_io_channel_ref(ret->incoming);

	return ret;
}

void irc_transport_disconnect(struct irc_transport *transport)
{
	if (transport->incoming == NULL)
		return; /* We're already disconnected */

	g_io_channel_unref(transport->incoming);

	g_source_remove(transport->incoming_id);
	if (transport->outgoing_id)
		g_source_remove(transport->outgoing_id);

	transport->incoming = NULL;

	transport->callbacks->disconnect(transport);
}

static void free_pending_line(void *_line, void *userdata)
{
	free_line((struct irc_line *)_line);
}

void free_irc_transport(struct irc_transport *transport)
{
	/* Should already be disconnected */
	g_assert(transport->incoming == NULL);
	g_free(transport->charset);

	if (transport->outgoing_iconv != (GIConv)-1)
		g_iconv_close(transport->outgoing_iconv);
	if (transport->incoming_iconv != (GIConv)-1)
		g_iconv_close(transport->incoming_iconv);

	g_assert(transport->pending_lines != NULL);
	g_queue_foreach(transport->pending_lines, free_pending_line, NULL);
	g_queue_free(transport->pending_lines);

	g_free(transport);
}

/**
 * Change the character set used to send data to a client
 * @param c client to change the character set for
 * @param name name of the character set to change to
 * @return whether changing the character set succeeded
 */
gboolean transport_set_charset(struct irc_transport *transport, const char *name)
{
	GIConv tmp;

	if (name != NULL) {
		tmp = g_iconv_open(name, "UTF-8");

		if (tmp == (GIConv)-1) {
			return FALSE;
		}
	} else {
		tmp = (GIConv)-1;
	}
	
	if (transport->outgoing_iconv != (GIConv)-1)
		g_iconv_close(transport->outgoing_iconv);

	transport->outgoing_iconv = tmp;

	if (name != NULL) {
		tmp = g_iconv_open("UTF-8", name);

		if (tmp == (GIConv)-1) {
			return FALSE;
		}
	} else {
		tmp = (GIConv)-1;
	}

	if (transport->incoming_iconv != (GIConv)-1)
		g_iconv_close(transport->incoming_iconv);

	transport->incoming_iconv = tmp;

	g_free(transport->charset);
	transport->charset = g_strdup(name);

	return TRUE;
}

static gboolean transport_send_queue(GIOChannel *ioc, GIOCondition cond, 
									  void *_transport)
{
	gboolean ret = FALSE;
	struct irc_transport *transport = _transport;
	GIOStatus status;

	g_assert(ioc == transport->incoming);

	status = g_io_channel_flush(transport->incoming, NULL);
	if (status == G_IO_STATUS_AGAIN)
		ret = TRUE;

	g_assert(transport->pending_lines != NULL);

	while (!g_queue_is_empty(transport->pending_lines)) {
		GError *error = NULL;
		struct irc_line *l = g_queue_pop_head(transport->pending_lines);

		g_assert(transport->incoming != NULL);
		status = irc_send_line(transport->incoming, 
							   transport->outgoing_iconv, l, &error);

		switch (status) {
		case G_IO_STATUS_AGAIN:
			g_queue_push_head(transport->pending_lines, l);
			return TRUE;
		case G_IO_STATUS_ERROR:
			transport->callbacks->log(transport, l, error);
			g_error_free(error);
			break;
		case G_IO_STATUS_EOF:
			transport->outgoing_id = 0;

			transport->callbacks->hangup(transport);

			free_line(l);

			return FALSE;
		case G_IO_STATUS_NORMAL:
			transport->last_line_sent = time(NULL);
			break;
		}

		status = g_io_channel_flush(transport->incoming, &error);
		switch (status) {
		case G_IO_STATUS_EOF:
			g_assert_not_reached();
		case G_IO_STATUS_AGAIN:
			free_line(l);
			return TRUE;
		case G_IO_STATUS_NORMAL:
			break;
		case G_IO_STATUS_ERROR:
			transport->callbacks->log(transport, l, error);
			g_error_free(error);
			break;
		}
		free_line(l);
	}

	if (!ret)
		transport->outgoing_id = 0;
	return ret;
}

gboolean transport_send_line(struct irc_transport *transport, 
							 const struct irc_line *l)
{
	GError *error = NULL;
	GIOStatus status;

	if (transport->incoming == NULL)
		return FALSE;

	if (transport->outgoing_id != 0) {
		g_queue_push_tail(transport->pending_lines, linedup(l));
		return TRUE;
	}

	status = irc_send_line(transport->incoming, transport->outgoing_iconv, l, &error);

	switch (status) {
	case G_IO_STATUS_AGAIN:
		transport->outgoing_id = g_io_add_watch(transport->incoming, G_IO_OUT, 
										transport_send_queue, transport);
		g_queue_push_tail(transport->pending_lines, linedup(l));
		break;
	case G_IO_STATUS_EOF:
		transport->callbacks->hangup(transport);
		return FALSE;
	case G_IO_STATUS_ERROR:
		transport->callbacks->log(transport, l, error);
		g_error_free(error);
		return FALSE;
	case G_IO_STATUS_NORMAL:
		transport->last_line_sent = time(NULL);
		break;
	}

	status = g_io_channel_flush(transport->incoming, &error);

	switch (status) {
	case G_IO_STATUS_EOF:
		g_assert_not_reached();
	case G_IO_STATUS_NORMAL:
		break;
	case G_IO_STATUS_AGAIN:
		transport->outgoing_id = g_io_add_watch(transport->incoming, G_IO_OUT, 
										transport_send_queue, transport);
		break;
	case G_IO_STATUS_ERROR:
		transport->callbacks->log(transport, l, error);
		g_error_free(error);
		return FALSE;
	}

	return TRUE;
}

gboolean transport_send_response(struct irc_transport *transport, const char *from, const char *to, int response, ...) 
{
	struct irc_line *l;
	gboolean ret;
	va_list ap;

	g_assert(transport != NULL);

	va_start(ap, response);
	l = virc_parse_response(from, to, response, ap);
	va_end(ap);

	ret = transport_send_line(transport, l);

	free_line(l); 

	return ret;
}

gboolean transport_send_args(struct irc_transport *transport, ...) 
{
	struct irc_line *l;
	gboolean ret;
	va_list ap;

	g_assert(transport != NULL);

	va_start(ap, transport);
	l = virc_parse_line(NULL, ap);
	va_end(ap);

	ret = transport_send_line(transport, l);

	free_line(l); 

	return ret;
}

void transport_parse_buffer(struct irc_transport *transport)
{
	handle_transport_receive(transport->incoming, 
			  g_io_channel_get_buffer_condition(transport->incoming) & G_IO_IN,
			  transport);

	/* g_assert(g_io_channel_get_buffer_condition(transport->incoming) == 0); */
}

gboolean transport_blocking_recv(struct irc_transport *transport, struct irc_line **l)
{
	GIOFlags old_flags = g_io_channel_get_flags(transport->incoming);
	GError *error = NULL;
	GIOStatus status;
	gboolean ret;

	g_io_channel_set_flags(transport->incoming, old_flags & ~G_IO_FLAG_NONBLOCK, &error);

	status = irc_recv_line(transport->incoming, transport->incoming_iconv, &error, l);

	g_io_channel_set_flags(transport->incoming, old_flags, &error);

	ret = handle_recv_status(transport, status, error);

	if (error != NULL)
		g_error_free(error);

	return ret;
}

char *transport_get_peer_hostname(struct irc_transport *transport)
{
	int fd;
	socklen_t len = sizeof(struct sockaddr_storage);
	struct sockaddr_storage sa;
	char hostname[NI_MAXHOST];

	fd = g_io_channel_unix_get_fd(transport->incoming);

	if (getpeername (fd, (struct sockaddr *)&sa, &len) < 0) {
		return NULL;
	}

	if (sa.ss_family == AF_INET || sa.ss_family == AF_INET6) {
		if (getnameinfo((struct sockaddr *)&sa, len, hostname, sizeof(hostname),
						NULL, 0, 0) == 0) {
			return g_strdup(hostname);
		} 
	} else if (sa.ss_family == AF_UNIX) {
		return g_strdup("localhost");
	}

	return NULL;
}