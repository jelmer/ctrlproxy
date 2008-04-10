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

static gboolean process_to_client(struct irc_client *c, const struct irc_line *l)
{
	log_client_line(c, l, FALSE);
	return TRUE;
}

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

	log_client_line(c, l, TRUE);

	l->origin = g_strdup(c->state->me.hostmask);

	if (!run_client_filter(c, l, TO_SERVER)) {
		g_free(l->origin);
		return TRUE;
	}

	g_assert(l->args[0] != NULL);

	if (!g_strcasecmp(l->args[0], "QUIT")) {
		client_disconnect(c, "Client exiting");
		g_free(l->origin);
		return FALSE;
	} else if (!g_strcasecmp(l->args[0], "PING")) {
		client_send_args(c, "PONG", c->network->info->name, l->args[1], NULL);
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
		ctcp_client_request_record(c, l);

		/* send off to server */
		network_send_line(c->network, c, l, TRUE);
	} else if (!g_strcasecmp(l->args[0], "NOTICE") && l->argc > 2 && 
			l->args[2][0] == '\001') {
		network_send_line(c->network, c, l, TRUE);
	} else if (!g_strcasecmp(l->args[0], "")) {
	} else if (c->network->connection.state == NETWORK_CONNECTION_STATE_MOTD_RECVD) {
		if (c->network->config->disable_cache || !client_try_cache(c, c->network->state, l)) {
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

/**
 * Send welcome information to a client, optionally disconnecting 
 * the client if it isn't welcome.
 *
 * @param client Client to talk to.
 * @return whether the client was accepted or refused
 */
static gboolean welcome_client(struct irc_client *client)
{
	char *features, *tmp;

	g_assert(client);

	client_send_response(client, RPL_WELCOME, "Welcome to the ctrlproxy", NULL);
	tmp = g_strdup_printf("Host %s is running ctrlproxy", client->default_origin);
	client_send_response(client, RPL_YOURHOST, tmp, NULL); 
	g_free(tmp);
	client_send_response(client, RPL_CREATED, 
		"Ctrlproxy (c) 2002-2008 Jelmer Vernooij <jelmer@samba.org>", NULL);
	client_send_response(client, RPL_MYINFO, 
		 client->network->info->name, 
		 ctrlproxy_version(), 
		 (client->network->state != NULL && client->network->info->supported_user_modes)?client->network->info->supported_user_modes:ALLMODES, 
		 (client->network->state != NULL && client->network->info->supported_channel_modes)?client->network->info->supported_channel_modes:ALLMODES,
		 NULL);

	features = network_generate_feature_string(client->network);

	client_send_response(client, RPL_BOUNCE, features, 
						 "are supported on this server", NULL);

	g_free(features);

	if (client->network->state != NULL) {
		client_send_luserchannels(client, g_list_length(client->network->state->channels));
	}

	tmp = g_strdup_printf("I have %d clients", g_list_length(client->network->clients));
	client_send_response(client, RPL_LUSERME, tmp, NULL);
	g_free(tmp);

	if (!client->network->global->config->motd_file) {
		client_send_motd(client, NULL);
	} else if (!strcmp(client->network->global->config->motd_file, "")) {
		client_send_motd(client, NULL);
	} else {
		char **lines = get_motd_lines(client->network->global->config->motd_file);
		client_send_motd(client, lines);
		g_strfreev(lines);
	}

	g_assert(client->state != NULL);
	g_assert(client->network != NULL);

	if (client->network->state != NULL) {
		if (g_strcasecmp(client->state->me.nick, client->network->state->me.nick) != 0) {
			/* Tell the client our his/her real nick */
			client_send_args_ex(client, client->state->me.hostmask, "NICK", 
								client->network->state->me.nick, NULL); 

			/* Try to get the nick the client specified */
			if (!client->network->config->ignore_first_nick) {
				network_send_args(client->network, "NICK", 
								  client->requested_nick, NULL);
			}
		}
	}

	if (!new_client_hook_execute(client)) {
		client_disconnect(client, "Refused by client connect hook");
		return FALSE;
	}

	client_replicate(client);

	return TRUE;
}

static void client_free_private(struct irc_client *c)
{
	network_unref(c->network);
}

static void handle_client_disconnect(struct irc_client *c)
{
	if (c->network != NULL)
		c->network->clients = g_list_remove(c->network->clients, c);
}

void log_client(enum log_level, const struct irc_client *, const char *data);
static struct irc_client_callbacks default_callbacks = {
	.process_from_client = process_from_client, 
	.process_to_client = process_to_client,
	.log_fn = log_client,
	.disconnect = handle_client_disconnect,
	.free_private_data = client_free_private,
	.welcome = welcome_client,
};

struct irc_client *client_init(struct irc_network *n, GIOChannel *c, const char *desc)
{
	struct irc_client *client;
	struct irc_transport *transport;

	g_io_channel_set_flags(c, G_IO_FLAG_NONBLOCK, NULL);
	g_io_channel_set_close_on_unref(c, TRUE);
	transport = irc_transport_new_iochannel(c);

	client = irc_client_new(transport, n?n->info->name:get_my_hostname(), desc, &default_callbacks);

	client->network = network_ref(n);

	if (n != NULL && n->global != NULL)
		client_set_charset(client, n->global->config->client_charset);

	client->exit_on_close = FALSE;
	
	/* parse any data currently in the buffer */
	client_parse_buffer(client);

	return client;
}

/**
 * Send a line to a list of clients.
 *
 * @param clients List of clients to send to
 * @param l Line to send
 * @param exception Client to which nothing should be sent. Can be NULL.
 */
void clients_send(GList *clients, const struct irc_line *l, 
				  const struct irc_client *exception) 
{
	GList *g;

	for (g = clients; g; g = g->next) {
		struct irc_client *c = (struct irc_client *)g->data;
		if (c != exception) {
			if (run_client_filter(c, l, FROM_SERVER)) { 
				client_send_line(c, l);
			}
		}
	}
}


void clients_send_args_ex(GList *clients, const char *hostmask, ...)
{
	struct irc_line *l;
	va_list ap;

	va_start(ap, hostmask);
	l = virc_parse_line(hostmask, ap);
	va_end(ap);

	clients_send(clients, l, NULL);

	free_line(l); l = NULL;
}



