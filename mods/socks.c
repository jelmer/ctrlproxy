/* 
	ctrlproxy: A modular IRC proxy
	(c) 2005 Jelmer Vernooij <jelmer@nl.linux.org>

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

/* TODO:
 *  - ipv6 support
 */

#define _GNU_SOURCE
#include "ctrlproxy.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <glib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "socks"

#define SOCKS_VERSION 0x05
#define DEFAULT_SOCKS_PORT 1080

static GIOChannel *server_channel = NULL;
static int server_channel_in = 0;

struct socks_client
{
	GIOChannel *connection;
	guint8 socks_version;
	const char *user;
	const char *password;
};

static gboolean handle_new_client (GIOChannel *ioc, GIOCondition o, gpointer data)
{
	
	return TRUE;
}

/* Configure port number to listen on */
gboolean fini_plugin(struct plugin *p)
{
	/* Close port */
	g_source_remove(server_channel_in);
	g_io_channel_unref(server_channel);
	return TRUE;
}

const char name_plugin[] = "socks";

gboolean init_plugin(struct plugin *p)
{
	int sock;
	const int on = 1;
	struct sockaddr_in addr;
	guint16 port;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		g_warning( "error creating socket: %s", strerror(errno));
		return FALSE;
	}

	port = DEFAULT_SOCKS_PORT;

	/* FIXME: Get port from configuration */

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	addr.sin_family = AF_INET;
	addr.sin_port = port;
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind (sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		g_warning("Unable to bind to port %d: %s", port, strerror(errno));
		return FALSE;
	}

	if (listen(sock, 5) < 0) {
		g_warning( "error listening on socket: %s", strerror(errno));
		return FALSE;
	}

	server_channel = g_io_channel_unix_new(sock);

	if (!server_channel) {
		g_warning("Unable to create GIOChannel for server socket");
		return FALSE;
	}

	server_channel_in = g_io_add_watch(server_channel, G_IO_IN, handle_new_client, NULL);
	
	return TRUE;
}
