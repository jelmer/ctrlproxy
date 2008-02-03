/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2007 Jelmer Vernooij <jelmer@nl.linux.org>

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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <netinet/in.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "internals.h"
#include "irc.h"


/**
 * Process incoming lines from a client.
 *
 * @param c Client to talk to
 * @param l Line received
 * @return Whether the line was processed correctly
 */
static gboolean process_from_client(struct irc_client *c, const struct irc_line *_l)
{
	struct irc_line ol, *l;
	g_assert(c != NULL);
	g_assert(_l != NULL);

	ol = *_l;
	l = &ol;

	l->origin = g_strdup(c->state->me.hostmask);

	if (!run_client_filter(c, l, TO_SERVER)) {
		g_free(l->origin);
		return TRUE;
	}

	g_assert(l->args[0] != NULL);

	if (!g_strcasecmp(l->args[0], "QUIT")) {
		disconnect_client(c, "Client exiting");
		g_free(l->origin);
		return FALSE;
	} else if (!g_strcasecmp(l->args[0], "PING")) {
		client_send_args(c, "PONG", c->network->info.name, l->args[1], NULL);
	} else if (!g_strcasecmp(l->args[0], "PONG")) {
		if (l->argc < 2) {
			client_send_response(c, ERR_NEEDMOREPARAMS, l->args[0], 
								 "Not enough parameters", NULL);
			g_free(l->origin);
			return TRUE;
		}
		c->last_pong = time(NULL);
	} else if (!g_strcasecmp(l->args[0], "USER") ||
			  !g_strcasecmp(l->args[0], "PASS")) {
		client_send_response(c, ERR_ALREADYREGISTERED,  
						 "Please register only once per session", NULL);
	} else if (!g_strcasecmp(l->args[0], "CTRLPROXY")) {
		admin_process_command(c, l, 1);
	} else if (c->network->global->config->admin_user != NULL && 
			   !g_strcasecmp(l->args[0], "PRIVMSG") && 
			   !g_strcasecmp(l->args[1], c->network->global->config->admin_user)) {
		admin_process_command(c, l, 2);
	} else if (!g_strcasecmp(l->args[0], "PRIVMSG") && l->argc > 2 && 
			l->args[2][0] == '\001' && 
			g_strncasecmp(l->args[2], "\001ACTION", 7) != 0) {
		ctcp_process_client_request(c, l);
	} else if (!g_strcasecmp(l->args[0], "NOTICE") && l->argc > 2 && 
			l->args[2][0] == '\001') {
		ctcp_process_client_reply(c, l);
	} else if (!g_strcasecmp(l->args[0], "")) {
	} else if (c->network->connection.state == NETWORK_CONNECTION_STATE_MOTD_RECVD) {
		if (c->network->config->disable_cache || !client_try_cache(c, l)) {
			/* Perhaps check for validity of input here ? It could save us some bandwidth 
			 * to the server, though very unlikely to occur often */
			network_send_line(c->network, c, l, FALSE);
		}
	} else if (c->network->connection.state == NETWORK_CONNECTION_STATE_NOT_CONNECTED) {
		char *msg;
		if (c->network->connection.data.tcp.last_disconnect_reason == NULL)
			msg = g_strdup("Currently not connected to server...");
		else
			msg = g_strdup_printf("Currently not connected to server... (%s)",
					c->network->connection.data.tcp.last_disconnect_reason);

		client_send_args(c, "NOTICE", client_get_default_target(c), msg, NULL);
		g_free(msg);
	}

	g_free(l->origin);
	return TRUE;
}

struct irc_client *client_init(struct irc_network *n, GIOChannel *c, const char *desc)
{
	struct irc_client *client = irc_client_new(c, desc, process_from_client, n);

	if (n != NULL && n->global != NULL)
		client_set_charset(client, n->global->config->client_charset);

	return client;
}
