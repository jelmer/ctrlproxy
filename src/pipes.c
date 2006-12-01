/* 
	ctrlproxy: A modular IRC proxy
	(c) 2006 Jelmer Vernooij <jelmer@nl.linux.org>
	
	Listening on pipes in ~/.ctrlproxy/pipes/NETWORK

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
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
#include <sys/un.h>

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

static gboolean handle_new_client(GIOChannel *c_server, GIOCondition condition, void *_network)
{
	struct network *network = _network;
	GIOChannel *c;
	int sock = accept(g_io_channel_unix_get_fd(c_server), NULL, 0);

	if (sock < 0) {
		log_global(NULL, LOG_WARNING, "Error accepting new connection: %s", strerror(errno));
		return TRUE;
	}

	c = g_io_channel_unix_new(sock);

	g_io_channel_set_close_on_unref(c, TRUE);
	g_io_channel_set_encoding(c, NULL, NULL);
	g_io_channel_set_flags(c, G_IO_FLAG_NONBLOCK, NULL);

	client_init(network, c, "Client on unix socket");
	g_io_channel_unref(c);

	return TRUE;
}

gboolean network_start_unix_pipe(struct network *n)
{
	int sock;
	struct sockaddr_un un;

	sock = socket(PF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) {
		log_global(NULL, LOG_ERROR, "error creating unix socket: %s", strerror(errno));
		return FALSE;
	}
	
	un.sun_family = AF_UNIX;
	snprintf(un.sun_path, sizeof(un.sun_path), "%s/%s", n->global->config->pipes_dir, n->name);
	unlink(un.sun_path);

	if (bind(sock, (struct sockaddr *)&un, sizeof(un)) < 0) {
		log_global(NULL, LOG_ERROR, "unable to bind to %s: %s", un.sun_path, strerror(errno));
		return FALSE;
	}
	
	if (listen(sock, 5) < 0) {
		log_global(NULL, LOG_ERROR, "error listening on socket: %s", strerror(errno));
		return FALSE;
	}

	n->connection.unix_incoming = g_io_channel_unix_new(sock);

	if (!n->connection.unix_incoming) {
		log_global(NULL, LOG_ERROR, "Unable to create GIOChannel for unix server socket");
		return FALSE;
	}

	g_io_channel_set_close_on_unref(n->connection.unix_incoming, TRUE);

	g_io_channel_set_encoding(n->connection.unix_incoming, NULL, NULL);
	n->connection.unix_incoming_id = g_io_add_watch(n->connection.unix_incoming, G_IO_IN, handle_new_client, n);
	g_io_channel_unref(n->connection.unix_incoming);

	return TRUE;
}

gboolean network_stop_unix_pipe(struct network *n)
{
	if (n->connection.unix_incoming_id > 0)
		g_source_remove(n->connection.unix_incoming_id);
	return TRUE;
}
