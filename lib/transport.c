
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

#include "transport.h"
#include "line.h"
#include "util.h"
#include <glib.h>

static gboolean transport_send_queue(GIOChannel *c, GIOCondition cond, 
										 void *_client);

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

	if (cond & G_IO_HUP) {
		transport->callbacks->disconnect(transport);
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

		if (status == G_IO_STATUS_EOF) {
			transport->callbacks->disconnect(transport);
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
		return ret;
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
	g_assert(transport->incoming != NULL);

	g_io_channel_shutdown(transport->incoming, FALSE, NULL);

	g_source_remove(transport->incoming_id);
	if (transport->outgoing_id)
		g_source_remove(transport->outgoing_id);
}

static void free_pending_line(void *_line, void *userdata)
{
	free_line((struct irc_line *)_line);
}

void free_irc_transport(struct irc_transport *transport)
{
	g_free(transport->charset);
	g_io_channel_unref(transport->incoming);
	
	transport->incoming = NULL;

	if (transport->outgoing_iconv != (GIConv)-1)
		g_iconv_close(transport->outgoing_iconv);
	if (transport->incoming_iconv != (GIConv)-1)
		g_iconv_close(transport->incoming_iconv);

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
	struct irc_transport *transport = _transport;

	g_assert(ioc == transport->incoming);

	while (!g_queue_is_empty(transport->pending_lines)) {
		GIOStatus status;
		GError *error = NULL;
		struct irc_line *l = g_queue_pop_head(transport->pending_lines);

		g_assert(transport->incoming != NULL);
		status = irc_send_line(transport->incoming, 
							   transport->outgoing_iconv, l, &error);

		if (status == G_IO_STATUS_AGAIN) {
			g_queue_push_head(transport->pending_lines, l);
			return TRUE;
		}

		if (status == G_IO_STATUS_ERROR) {
			transport->callbacks->log(transport, l, error);
		} else if (status == G_IO_STATUS_EOF) {
			transport->outgoing_id = 0;

			transport->callbacks->disconnect(transport);

			free_line(l);

			return FALSE;
		}

		free_line(l);
	}

	transport->outgoing_id = 0;
	return FALSE;
}

gboolean transport_send_line(struct irc_transport *transport, 
							 const struct irc_line *l)
{
	if (transport->outgoing_id == 0) {
		GError *error = NULL;
		GIOStatus status = irc_send_line(transport->incoming, transport->outgoing_iconv, l, &error);

		if (status == G_IO_STATUS_AGAIN) {
			transport->outgoing_id = g_io_add_watch(transport->incoming, G_IO_OUT, 
											transport_send_queue, transport);
			g_queue_push_tail(transport->pending_lines, linedup(l));
		} else if (status == G_IO_STATUS_EOF) {
			transport->callbacks->disconnect(transport);
			return FALSE;
		} else if (status == G_IO_STATUS_ERROR) {
			transport->callbacks->log(transport, l, error);
		} 

		return TRUE;
	}

	g_queue_push_tail(transport->pending_lines, linedup(l));

	return TRUE;
}

gboolean transport_send_args(struct irc_transport *transport, ...) 
{
	struct irc_line *l;
	gboolean ret;
	va_list ap;

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
			  g_io_channel_get_buffer_condition(transport->incoming),
			  transport);
}
