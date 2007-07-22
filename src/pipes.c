/* 
	ctrlproxy: A modular IRC proxy
	(c) 2006-2007 Jelmer Vernooij <jelmer@nl.linux.org>
	
	Listening on unix socket in ~/.ctrlproxy/socket

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

static gboolean handle_new_client(GIOChannel *c_server, GIOCondition condition, void *_global)
{
	GIOChannel *c;
	int sock = accept(g_io_channel_unix_get_fd(c_server), NULL, 0);

	if (sock < 0) {
		log_global(LOG_WARNING, "Error accepting new connection: %s", strerror(errno));
		return TRUE;
	}

	c = g_io_channel_unix_new(sock);

	g_io_channel_set_close_on_unref(c, TRUE);
	g_io_channel_set_encoding(c, NULL, NULL);
	g_io_channel_set_flags(c, G_IO_FLAG_NONBLOCK, NULL);

	client_init(NULL, c, "Client on unix socket");
	g_io_channel_unref(c);

	return TRUE;
}

gboolean start_unix_socket(struct global *global)
{
	int sock;
	struct sockaddr_un un;

	sock = socket(PF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) {
		log_global(LOG_ERROR, "error creating unix socket: %s", strerror(errno));
		return FALSE;
	}
	
	un.sun_family = AF_UNIX;
	strncpy(un.sun_path, global->config->network_socket, sizeof(un.sun_path));
	unlink(un.sun_path);

	if (bind(sock, (struct sockaddr *)&un, sizeof(un)) < 0) {
		log_global(LOG_ERROR, "unable to bind to %s: %s", un.sun_path, strerror(errno));
		return FALSE;
	}
	
	if (listen(sock, 5) < 0) {
		log_global(LOG_ERROR, "error listening on socket: %s", strerror(errno));
		return FALSE;
	}

	global->unix_incoming = g_io_channel_unix_new(sock);

	if (!global->unix_incoming) {
		log_global(LOG_ERROR, "Unable to create GIOChannel for unix server socket");
		return FALSE;
	}

	g_io_channel_set_close_on_unref(global->unix_incoming, TRUE);

	g_io_channel_set_encoding(global->unix_incoming, NULL, NULL);
	global->unix_incoming_id = g_io_add_watch(global->unix_incoming, G_IO_IN, handle_new_client, global);
	g_io_channel_unref(global->unix_incoming);

	return TRUE;
}

gboolean stop_unix_socket(struct global *global)
{
	if (global->unix_incoming_id > 0)
		g_source_remove(global->unix_incoming_id);
	unlink(global->config->network_socket);
	return TRUE;
}
