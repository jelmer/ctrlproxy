/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2007 Jelmer Vernooĳ <jelmer@jelmer.uk>

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

static void handle_offline_command(struct irc_client *c, const struct irc_line *l, const char *offline_reason)
{
	if (!base_strcmp(l->args[0], "PRIVMSG") || !base_strcmp(l->args[0], "NOTICE")) {
		client_send_response(c, ERR_NOSUCHNICK, l->args[1], offline_reason, NULL);
	} else if (!base_strcmp(l->args[0], "JOIN")) {
		/* Make network->internal_state join channel */
		client_send_response(c, ERR_NOSUCHCHANNEL, l->args[1], offline_reason, NULL);
	} else if (!base_strcmp(l->args[0], "PART")) {
		/* Make network->internal_state part channel */
		client_send_response(c, ERR_NOTONCHANNEL, l->args[1], offline_reason, NULL);
	} else {
		client_send_response(c, ERR_UNKNOWNCOMMAND, l->args[0], offline_reason, NULL);
	}
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

	g_assert(l->args[0] != NULL);

	if (!base_strcmp(l->args[0], "QUIT")) {
		client_disconnect(c, "Client exiting");
		g_free(l->origin);
		return FALSE;
	} else if (!base_strcmp(l->args[0], "PING")) {
		client_send_args(c, "PONG", c->network->name, l->args[1], NULL);
	} else if (!base_strcmp(l->args[0], "PONG")) {
		if (l->argc < 2) {
			client_send_response(c, ERR_NEEDMOREPARAMS, l->args[0],
								 "Not enough parameters", NULL);
			g_free(l->origin);
			return TRUE;
		}
		c->last_pong = time(NULL);
	} else if (!base_strcmp(l->args[0], "USER") ||
			  !base_strcmp(l->args[0], "PASS")) {
		client_send_response(c, ERR_ALREADYREGISTERED,
						 "Please register only once per session", NULL);
	} else if (!base_strcmp(l->args[0], "CTRLPROXY")) {
		admin_process_command(c, l, 1);
	} else if (c->network->global->config->admin_user != NULL &&
			   !base_strcmp(l->args[0], "PRIVMSG") &&
			   !base_strcmp(l->args[1], c->network->global->config->admin_user)) {
		admin_process_command(c, l, 2);
	} else if (!base_strcmp(l->args[0], "PRIVMSG") && l->argc > 2 &&
			l->args[2][0] == '\001' &&
			base_strncmp(l->args[2], "\001ACTION", 7) != 0) {
		ctcp_client_request_record(c, l);

		/* send off to server */
		network_forward_line(c->network, c, l, TRUE);
	} else if (!base_strcmp(l->args[0], "NOTICE") && l->argc > 2 &&
			l->args[2][0] == '\001') {
		network_forward_line(c->network, c, l, TRUE);
	} else if (!base_strcmp(l->args[0], "")) {
	} else if (c->network->connection.state == NETWORK_CONNECTION_STATE_MOTD_RECVD) {
		struct network_config *nc = c->network->private_data;

		if (nc->disable_cache || !client_try_cache(c, c->network->external_state, l, &c->network->global->config->cache)) {
			/* Perhaps check for validity of input here ? It could save us some bandwidth
			 * to the server, though unlikely to occur often */
			network_forward_line(c->network, c, l, FALSE);
		}
	} else if (c->network->connection.state == NETWORK_CONNECTION_STATE_NOT_CONNECTED) {
		char *msg;
		if (c->network->connection.data.tcp.last_disconnect_reason == NULL) {
			msg = g_strdup("Currently not connected to server...");
		} else {
			msg = g_strdup_printf("Currently not connected to server... (%s)",
					c->network->connection.data.tcp.last_disconnect_reason);
		}

		handle_offline_command(c, l, msg);

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
	struct network_config *nc;

	g_assert(client != NULL);

	if (client->network == NULL) {
		client_disconnect(client,
			  "Please select a network first, or specify one in your configuration file");
		return FALSE;
	}

	client->network->clients = g_list_append(client->network->clients, client);
	client_send_response(client, RPL_WELCOME, "Welcome to the ctrlproxy", NULL);
	tmp = g_strdup_printf("Host %s is running ctrlproxy", client->default_origin);
	client_send_response(client, RPL_YOURHOST, tmp, NULL);
	g_free(tmp);
	client_send_response(client, RPL_CREATED,
		"Ctrlproxy (c) 2002-2009 Jelmer Vernooĳ <jelmer@jelmer.uk>", NULL);
	client_send_response(client, RPL_MYINFO,
		 client->network->name,
		 ctrlproxy_version(),
		 (client->network->external_state != NULL && client->network->info->supported_user_modes)?client->network->info->supported_user_modes:ALLMODES,
		 (client->network->external_state != NULL && client->network->info->supported_channel_modes)?client->network->info->supported_channel_modes:ALLMODES,
		 NULL);

	features = network_generate_feature_string(client->network);

	client_send_response(client, RPL_BOUNCE, features,
						 "are supported on this server", NULL);

	g_free(features);

	if (client->network->external_state != NULL) {
		client_send_luserchannels(client, g_list_length(client->network->external_state->channels));
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

	if (client->network->external_state != NULL) {
		if (irccmp(client->state->info, client->state->me.nick, client->network->external_state->me.nick) != 0) {
			/* Tell the client our his/her real nick */
			client_send_args_ex(client, client->state->me.hostmask, "NICK",
								client->network->external_state->me.nick, NULL);

			/* Try to get the nick the client specified */
			nc = client->network->private_data;
			if (!nc->ignore_first_nick) {
				network_send_args(client->network, "NICK",
								  client->login_details->nick, NULL);
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
	irc_network_unref(c->network);
}

struct lose_client_hook_data {
	char *name;
	lose_client_hook hook;
	void *userdata;
};

GList *lose_client_hooks = NULL;


void add_lose_client_hook(const char *name, lose_client_hook h, void *userdata)
{
	struct lose_client_hook_data *d;

	d = g_malloc(sizeof(struct lose_client_hook_data));
	d->name = g_strdup(name);
	d->hook = h;
	d->userdata = userdata;
	lose_client_hooks = g_list_append(lose_client_hooks, d);
}

void del_lose_client_hook(const char *name)
{
	GList *l;
	for (l = lose_client_hooks; l; l = l->next) {
		struct lose_client_hook_data *d = (struct lose_client_hook_data *)l->data;
		if (!strcmp(d->name, name)) {
			g_free(d->name);
			lose_client_hooks = g_list_remove(lose_client_hooks, d);
			g_free(d);
			return;
		}
	}
}

static void handle_client_disconnect(struct irc_client *c)
{
	GList *l;

	for (l = lose_client_hooks; l; l = l->next) {
		struct lose_client_hook_data *d = (struct lose_client_hook_data *)l->data;
		d->hook(c, d->userdata);
	}

	if (c->network != NULL) {
		c->network->clients = g_list_remove(c->network->clients, c);
	}
}

static void client_connect_command(struct irc_client *client, const char *hostname, guint16 port)
{
	extern struct global *my_global;

	struct irc_network *network;

	network = irc_network_ref(find_network_by_hostname(my_global,
												   hostname, port, my_global->config->create_implicit,
												   client->login_details));

	if (network == NULL) {
		client_log(LOG_ERROR, client,
			"Unable to connect to network with name %s",
			hostname);
		client->network = NULL;
		return;
	}

	if (network->connection.state == NETWORK_CONNECTION_STATE_NOT_CONNECTED) {
		client_send_args(client, "NOTICE",
							client_get_default_target(client),
							"Connecting to network", NULL);
		connect_network(network);
	}

	client->network = network;
}

void log_client(enum log_level, const struct irc_client *, const char *data);
static struct irc_client_callbacks default_callbacks = {
	.process_from_client = process_from_client,
	.process_to_client = process_to_client,
	.log_fn = log_client,
	.disconnect = handle_client_disconnect,
	.free_private_data = client_free_private,
	.welcome = welcome_client,
	.on_connect = client_connect_command,
};

struct irc_client *client_init_iochannel(struct irc_network *n, GIOChannel *c, const char *desc)
{
	struct irc_transport *transport;
	struct irc_client *client;
	g_io_channel_set_flags(c, G_IO_FLAG_NONBLOCK, NULL);
	g_io_channel_set_close_on_unref(c, TRUE);
	transport = irc_transport_new_iochannel(c);

	client = client_init(n, transport, desc);

	return client;
}

struct irc_client *client_init(struct irc_network *n, struct irc_transport *transport, const char *desc)
{
	struct irc_client *client;

	client = irc_client_new(
		transport,
		n != NULL?n->name:get_my_hostname(), desc, &default_callbacks);
	client->authenticated = FALSE;

	client->network = irc_network_ref(n);

	if (n != NULL && n->global != NULL) {
		if (!client_set_charset(client, n->global->config->client_charset)) {
			client_disconnect(client, "Unable to set character set.");
			return NULL;
		}
	}

	client->exit_on_close = FALSE;

	return client;
}

struct irc_line *irc_line_replace_hostmask(const struct irc_line *l,
							   const struct irc_network_info *info,
							   const struct network_nick *old,
							   const struct network_nick *new)
{
	struct irc_line *ret;

	if (irccmp(info, old->hostmask, new->hostmask) == 0) {
		return NULL; /* No need to replace anything */
	}

	/* Replace lines "faked" to be from the user itself */
	if (l->origin != NULL && line_from_nick(info, l, old->nick)) {
		ret = linedup(l);
		g_free(ret->origin);
		ret->origin = g_strdup(new->hostmask);
		return ret;
	}
	switch (irc_line_respcode(l)) {
		case RPL_USERHOST:
		if (strstr(l->args[2], old->hostname)) {
			int i;
			gchar **users = g_strsplit(g_strstrip(l->args[2]), " ", 0);
			for (i = 0; users[i]; i++) {
				gchar** tmp302 = g_strsplit(users[i], "=", 2);
				if (g_strv_length(tmp302) > 1) {
					/* FIXME: strip *'s from the end of tmp302[0] */
					if (!irccmp(info, tmp302[0], old->nick)) {
						g_free(users[i]);
						users[i] = g_strdup_printf("%s=%c%s@%s",
								tmp302[0], tmp302[1][0],
								new->username, new->hostname);
					}
				}
				g_strfreev(tmp302);
			}
			ret = linedup(l);
			g_free(ret->args[2]);
			ret->args[2] = g_strjoinv(" ", users);
			g_strfreev(users);
			return ret;
		}
		break;
		case RPL_WHOREPLY:
		if (l->argc >= 6) {
			if (!irccmp(info, l->args[6], old->nick)) {
				ret = linedup(l);
				g_free(ret->args[4]);
				ret->args[4] = g_strdup(new->hostname);
				g_free(ret->args[3]);
				ret->args[3] = g_strdup(new->username);
				return ret;
			}
		}
		break;
		case RPL_WHOISUSER:
		case RPL_WHOWASUSER:
		if (l->argc >= 4) {
			if (!irccmp(info, l->args[2], old->nick)) {
				ret = linedup(l);
				g_free(ret->args[4]);
				ret->args[4] = g_strdup(new->hostname);
				g_free(ret->args[3]);
				ret->args[3] = g_strdup(new->username);
				return ret;
			}
		}
	}
	return NULL;
}

/**
 * Forward a server line to a client.
 * This will optionally make sure the local hostmask is sent.
 */
static gboolean client_forward_from_server(struct irc_client *c, const struct irc_line *l)
{
	struct irc_line *nl;
	gboolean ret;

	/* Make sure the client only sees its only hostmask */
	if (c->network->external_state != NULL) {
		nl = irc_line_replace_hostmask(l,
								  c->network->info,
								  &c->network->external_state->me,
								  &c->state->me);
		if (nl != NULL) {
			l = nl;
		}
	} else {
		nl = NULL;
	}

	ret = client_send_line(c, l, NULL);
	free_line(nl);
	return ret;
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
		if (c == exception) {
			continue;
		}

		client_forward_from_server(c, l);
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
