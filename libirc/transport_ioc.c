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

struct irc_transport_data_iochannel {
	GIOChannel *incoming;
	gint incoming_id;
	gint outgoing_id;
	GIConv incoming_iconv;
	GIConv outgoing_iconv;
	GQueue *pending_lines;
};


static gboolean handle_transport_receive(GIOChannel *c, GIOCondition cond, 
									  void *_transport);

static void free_pending_line(void *_line, void *userdata)
{
	free_line((struct irc_line *)_line);
}

static void irc_transport_iochannel_free_data(void *data)
{
	struct irc_transport_data_iochannel *backend_data = (struct irc_transport_data_iochannel *)data;

	g_assert(backend_data->incoming == NULL);

	if (backend_data->outgoing_iconv != (GIConv)-1)
		g_iconv_close(backend_data->outgoing_iconv);
	if (backend_data->incoming_iconv != (GIConv)-1)
		g_iconv_close(backend_data->incoming_iconv);

	g_queue_foreach(backend_data->pending_lines, free_pending_line, NULL);
	g_queue_free(backend_data->pending_lines);

	g_free(backend_data);
}

static char *irc_transport_iochannel_get_peer_name(void *data)
{
	int fd;
	socklen_t len = sizeof(struct sockaddr_storage);
	struct sockaddr_storage sa;
	char hostname[NI_MAXHOST];
	struct irc_transport_data_iochannel *backend_data = (struct irc_transport_data_iochannel *)data;

	fd = g_io_channel_unix_get_fd(backend_data->incoming);

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

static gboolean irc_transport_iochannel_set_charset(struct irc_transport *transport, const char *name)
{
	GIConv tmp;
	struct irc_transport_data_iochannel *backend_data = (struct irc_transport_data_iochannel *)transport->backend_data;

	if (name != NULL) {
		tmp = g_iconv_open(name, "UTF-8");

		if (tmp == (GIConv)-1) {
			return FALSE;
		}
	} else {
		tmp = (GIConv)-1;
	}
	
	if (backend_data->outgoing_iconv != (GIConv)-1)
		g_iconv_close(backend_data->outgoing_iconv);

	backend_data->outgoing_iconv = tmp;

	if (name != NULL) {
		tmp = g_iconv_open("UTF-8", name);

		if (tmp == (GIConv)-1) {
			return FALSE;
		}
	} else {
		tmp = (GIConv)-1;
	}

	if (backend_data->incoming_iconv != (GIConv)-1)
		g_iconv_close(backend_data->incoming_iconv);

	backend_data->incoming_iconv = tmp;

	return TRUE;
}

static void irc_transport_iochannel_disconnect(void *data)
{
	struct irc_transport_data_iochannel *backend_data = (struct irc_transport_data_iochannel *)data;
	g_io_channel_unref(backend_data->incoming);

	g_source_remove(backend_data->incoming_id);
	if (backend_data->outgoing_id)
		g_source_remove(backend_data->outgoing_id);

	backend_data->incoming = NULL;
}

static gboolean transport_send_queue(GIOChannel *ioc, GIOCondition cond, 
									  void *_transport);

static gboolean irc_transport_iochannel_send_line(struct irc_transport *transport, const struct irc_line *l, GError **error)
{
	GIOStatus status;
	GError *tmp = NULL;

	struct irc_transport_data_iochannel *backend_data = (struct irc_transport_data_iochannel *)transport->backend_data;

	if (error == NULL) {
		error = &tmp;
	}

	if (backend_data->outgoing_id != 0) {
		g_queue_push_tail(backend_data->pending_lines, linedup(l));
		return TRUE;
	}

	status = irc_send_line(backend_data->incoming, backend_data->outgoing_iconv, l, error);

	switch (status) {
	case G_IO_STATUS_AGAIN:
		backend_data->outgoing_id = g_io_add_watch(backend_data->incoming, G_IO_OUT, 
										transport_send_queue, transport);
		g_queue_push_tail(backend_data->pending_lines, linedup(l));
		break;
	case G_IO_STATUS_EOF:
		transport->callbacks->hangup(transport);
		return FALSE;
	case G_IO_STATUS_ERROR:
		transport->callbacks->log(transport, l, *error);
		if (error == &tmp)
			g_error_free(tmp);
		return FALSE;
	case G_IO_STATUS_NORMAL:
		transport->last_line_sent = time(NULL);
		break;
	}

	status = g_io_channel_flush(backend_data->incoming, error);

	switch (status) {
	case G_IO_STATUS_EOF:
		g_assert_not_reached();
	case G_IO_STATUS_NORMAL:
		break;
	case G_IO_STATUS_AGAIN:
		backend_data->outgoing_id = g_io_add_watch(backend_data->incoming, G_IO_OUT, 
										transport_send_queue, transport);
		break;
	case G_IO_STATUS_ERROR:
		transport->callbacks->log(transport, l, *error);
		if (error == &tmp)
			g_error_free(tmp);
		return FALSE;
	}

	return TRUE;

}

static void irc_transport_iochannel_activate(struct irc_transport *transport)
{
	struct irc_transport_data_iochannel *backend_data = (struct irc_transport_data_iochannel *)transport->backend_data;
	backend_data->incoming_id = g_io_add_watch(
							backend_data->incoming, G_IO_IN | G_IO_HUP, 
							handle_transport_receive, transport);

	handle_transport_receive(backend_data->incoming, 
			  g_io_channel_get_buffer_condition(backend_data->incoming) & G_IO_IN,
			  transport);
}



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
	struct irc_transport_data_iochannel *backend_data = (struct irc_transport_data_iochannel *)transport->backend_data;
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
		
		while ((status = irc_recv_line(c, backend_data->incoming_iconv, &error, 
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



static gboolean irc_transport_iochannel_is_connected(void *data)
{
	struct irc_transport_data_iochannel *backend_data = (struct irc_transport_data_iochannel *)data;

	return backend_data->incoming != NULL;
}

static const struct irc_transport_ops irc_transport_iochannel_ops = {
	.free_data = irc_transport_iochannel_free_data,
	.is_connected = irc_transport_iochannel_is_connected,
	.disconnect = irc_transport_iochannel_disconnect,
	.send_line = irc_transport_iochannel_send_line,
	.get_peer_name = irc_transport_iochannel_get_peer_name,
	.activate = irc_transport_iochannel_activate,
	.set_charset = irc_transport_iochannel_set_charset,
};

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
	struct irc_transport_data_iochannel *backend_data = g_new0(struct irc_transport_data_iochannel, 1);
	
	ret->backend_ops = &irc_transport_iochannel_ops;
	ret->backend_data = backend_data;
	backend_data->incoming = iochannel;
	backend_data->pending_lines = g_queue_new();
	backend_data->outgoing_iconv = backend_data->incoming_iconv = (GIConv)-1;
	g_io_channel_ref(backend_data->incoming);

	return ret;
}

static gboolean transport_send_queue(GIOChannel *ioc, GIOCondition cond, 
									  void *_transport)
{
	gboolean ret = FALSE;
	struct irc_transport *transport = _transport;
	struct irc_transport_data_iochannel *backend_data = (struct irc_transport_data_iochannel *)transport->backend_data;
	GIOStatus status;

	g_assert(ioc == backend_data->incoming);

	status = g_io_channel_flush(backend_data->incoming, NULL);
	if (status == G_IO_STATUS_AGAIN)
		ret = TRUE;

	g_assert(backend_data->pending_lines != NULL);

	while (!g_queue_is_empty(backend_data->pending_lines)) {
		GError *error = NULL;
		struct irc_line *l = g_queue_pop_head(backend_data->pending_lines);

		g_assert(backend_data->incoming != NULL);
		status = irc_send_line(backend_data->incoming, 
							   backend_data->outgoing_iconv, l, &error);

		switch (status) {
		case G_IO_STATUS_AGAIN:
			g_queue_push_head(backend_data->pending_lines, l);
			return TRUE;
		case G_IO_STATUS_ERROR:
			transport->callbacks->log(transport, l, error);
			g_error_free(error);
			break;
		case G_IO_STATUS_EOF:
			backend_data->outgoing_id = 0;

			transport->callbacks->hangup(transport);

			free_line(l);

			return FALSE;
		case G_IO_STATUS_NORMAL:
			transport->last_line_sent = time(NULL);
			break;
		}

		status = g_io_channel_flush(backend_data->incoming, &error);
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
		backend_data->outgoing_id = 0;
	return ret;
}


