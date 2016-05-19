
/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2008 Jelmer Vernooij <jelmer@jelmer.uk>

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

void irc_transport_disconnect(struct irc_transport *transport)
{
	if (!transport->backend_ops->is_connected(transport->backend_data))
		return; /* We're already disconnected */

	transport->backend_ops->disconnect(transport->backend_data);
	transport->callbacks->disconnect(transport);
}

void free_irc_transport(struct irc_transport *transport)
{
	transport->backend_ops->free_data(transport->backend_data);
	transport->backend_data = NULL;

	g_free(transport->charset);

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
	gboolean ret;

	ret = transport->backend_ops->set_charset(transport, name);
	if (ret == FALSE)
		return ret;

	g_free(transport->charset);
	transport->charset = g_strdup(name);

	return TRUE;
}

gboolean transport_send_line(struct irc_transport *transport, 
							 const struct irc_line *l, GError **error)
{
	if (!transport->backend_ops->is_connected(transport->backend_data)) {
		g_set_error_literal(error, IRC_TRANSPORT_ERROR, IRC_TRANSPORT_ERROR_DISCONNECTED,
							"Transport is disconnected");
		return FALSE;
	}

	return transport->backend_ops->send_line(transport, l, error);
}

gboolean transport_send_response(struct irc_transport *transport, GError **error, const char *from, const char *to, int response, ...) 
{
	struct irc_line *l;
	gboolean ret;
	va_list ap;

	g_assert(transport != NULL);

	va_start(ap, response);
	l = virc_parse_response(from, to, response, ap);
	va_end(ap);

	ret = transport_send_line(transport, l, error);

	free_line(l); 

	return ret;
}

gboolean transport_send_args(struct irc_transport *transport, GError **error, ...) 
{
	struct irc_line *l;
	gboolean ret;
	va_list ap;

	g_assert(transport != NULL);

	va_start(ap, error);
	l = virc_parse_line(NULL, ap);
	va_end(ap);

	ret = transport_send_line(transport, l, error);

	free_line(l); 

	return ret;
}

char *transport_get_peer_hostname(struct irc_transport *transport)
{
	return transport->backend_ops->get_peer_name(transport->backend_data);
}

void irc_transport_set_callbacks(struct irc_transport *transport, const struct irc_transport_callbacks *callbacks, void *userdata)
{
	transport->userdata = userdata;
	transport->callbacks = callbacks;

	transport->backend_ops->activate(transport);
}

GQuark irc_transport_error_quark(void)
{
	return g_quark_from_static_string("irc-transport-error-quark");
}
