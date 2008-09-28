/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2006 Jelmer Vernooij <jelmer@nl.linux.org>

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <pwd.h>
#include "internals.h"
#include "daemon/client.h"
#include "daemon/user.h"

static GList *daemon_clients = NULL;

void daemon_client_kill(struct daemon_client *dc)
{
	daemon_clients = g_list_remove(daemon_clients, dc);

	if (dc->freed)
		return;

	dc->freed = TRUE;

	if (dc->backend_transport != NULL) {
		irc_transport_disconnect(dc->backend_transport);
		free_irc_transport(dc->backend_transport);
		dc->backend_transport = NULL;
	}

	if (dc->client_transport != NULL) {
		irc_transport_disconnect(dc->client_transport);
		free_irc_transport(dc->client_transport);
		dc->client_transport = NULL;
	}

	daemon_user_free(dc->user);
	g_free(dc->nick);
	g_free(dc->password);
	g_free(dc->realname);
	g_free(dc->servername);
	g_free(dc->servicename);
	g_free(dc->unused);
	g_free(dc->username);
	g_free(dc->description);
	g_free(dc);

	if (dc->inetd)
		exit(0);
}

void daemon_client_send_credentials(struct daemon_client *dc)
{
	g_assert(dc->backend_transport != NULL);

	if (dc->servername != NULL && dc->servicename != NULL)
		transport_send_args(dc->backend_transport, "CONNECT", dc->servername, dc->servicename, NULL);
	transport_send_args(dc->backend_transport, "USER", dc->username, dc->mode, dc->unused, dc->realname, NULL);
	transport_send_args(dc->backend_transport, "NICK", dc->nick, NULL);

	daemon_clients = g_list_append(daemon_clients, dc);
}

void daemon_clients_exit()
{
	while (daemon_clients != NULL) {
		struct daemon_client *dc = daemon_clients->data;
		transport_send_args(dc->client_transport, "ERROR", "Server exiting", NULL);
		daemon_client_kill(dc);
	}
}
