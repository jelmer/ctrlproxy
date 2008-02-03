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

#include "internals.h"
#include "irc.h"
#include "ssl.h"

/**
 * Update the isupport settings for a local network based on the 
 * ISUPPORT info sent by a server.
 *
 * @param net_info Current information
 * @param remote_info Remote information
 * @return Whether updating went ok.
 */
static gboolean network_update_isupport(struct irc_network_info *net_info,
										struct irc_network_info *remote_info)
{
	if (remote_info->name != NULL) {
		g_free(net_info->name);
		net_info->name = g_strdup(remote_info->name);
	}

	if (remote_info->prefix != NULL) {
		g_free(net_info->prefix);
		net_info->prefix = g_strdup(remote_info->prefix);
	}

	if (remote_info->chantypes != NULL) {
		g_free(net_info->chantypes);
		net_info->chantypes = g_strdup(remote_info->chantypes);
	}

	if (remote_info->casemapping != CASEMAP_UNKNOWN) {
		net_info->casemapping = remote_info->casemapping;
	}

	if (remote_info->supported_user_modes != NULL) {
		g_free(net_info->supported_user_modes);
		net_info->supported_user_modes = g_strdup(remote_info->supported_user_modes);
	} 

	if (remote_info->supported_channel_modes != NULL) {
		g_free(net_info->supported_channel_modes);
		net_info->supported_channel_modes = g_strdup(remote_info->supported_channel_modes);
	}

	return TRUE;
}


/**
 * Process a line received from the server
 *
 * @param n Network the line is received at
 * @param l Line received
 * @return Whether the message was received ok
 */
static gboolean process_from_server(struct irc_network *n, const struct irc_line *l)
{
	struct irc_line *lc;
	GError *error = NULL;

	g_assert(n);
	g_assert(l);

	log_network_line(n, l, TRUE);

	/* Silently drop empty messages, as allowed by RFC */
	if (l->argc == 0) {
		return TRUE;
	}

	n->connection.last_line_recvd = time(NULL);

	if (n->state == NULL) {
		network_log(LOG_WARNING, n, 
					"Dropping message '%s' because network is disconnected.", 
					l->args[0]);
		return FALSE;
	}

	run_log_filter(n, lc = linedup(l), FROM_SERVER); free_line(lc);
	run_replication_filter(n, lc = linedup(l), FROM_SERVER); free_line(lc);

	g_assert(n->state);

	state_handle_data(n->state, l);

	g_assert(l->args[0]);

	if (!g_strcasecmp(l->args[0], "PING")){
		network_send_args(n, "PONG", l->args[1], NULL);
		return TRUE;
	} else if (!g_strcasecmp(l->args[0], "PONG")){
		return TRUE;
	} else if (!g_strcasecmp(l->args[0], "ERROR")) {
		network_log(LOG_ERROR, n, "error: %s", l->args[1]);
	} else if (!g_strcasecmp(l->args[0], "433") && 
			  n->connection.state == NETWORK_CONNECTION_STATE_LOGIN_SENT){
		char *tmp = g_strdup_printf("%s_", l->args[2]);
		network_send_args(n, "NICK", tmp, NULL);
		network_log(LOG_WARNING, n, "%s was already in use, trying %s", 
					l->args[2], tmp);
		g_free(tmp);
	} else if (atoi(l->args[0]) == RPL_ENDOFMOTD ||
			  atoi(l->args[0]) == ERR_NOMOTD) {
		GList *gl;
		n->connection.state = NETWORK_CONNECTION_STATE_MOTD_RECVD;

		/* Always save networks we've successfully connected to. */
		n->config->implicit = 0;

		network_set_charset(n, get_charset(&n->info));

		if (error != NULL)
			network_log(LOG_WARNING, n, "Error setting charset %s: %s", 
				get_charset(&n->info), error->message);

		network_log(LOG_INFO, n, "Successfully logged in");

		network_update_isupport(&n->info, &n->state->info);

		nickserv_identify_me(n, n->state->me.nick);

		clients_send_state(n->clients, n->state);

		server_connected_hook_execute(n);

		network_send_args(n, "USERHOST", n->state->me.nick, NULL);

		/* Rejoin channels */
		for (gl = n->config->channels; gl; gl = gl->next) {
			struct channel_config *c = gl->data;

			if (c->autojoin) {
				network_send_args(n, "JOIN", c->name, c->key, NULL);
			} 
		}
	} 

	if (n->connection.state == NETWORK_CONNECTION_STATE_MOTD_RECVD) {
		gboolean linestack_store = TRUE;
		if (atoi(l->args[0])) {
			linestack_store &= (!redirect_response(n, l));
		} else {
			if (n->clients == NULL) {
				if (!g_strcasecmp(l->args[0], "PRIVMSG") && l->argc > 2 && 
					l->args[2][0] == '\001' && 
					g_strncasecmp(l->args[2], "\001ACTION", 7) != 0) {
					ctcp_process_network_request(n, l);
				} else if (!g_strcasecmp(l->args[0], "NOTICE") && l->argc > 2 && 
					l->args[2][0] == '\001') {
					ctcp_process_network_reply(n, l);
				}
			} else if (run_server_filter(n, l, FROM_SERVER)) {
				if (!strcmp(l->args[0], "PRIVMSG") && 
					n->global->config->report_time == REPORT_TIME_ALWAYS)
					l = line_prefix_time(l, time(NULL)+n->global->config->report_time_offset);

				clients_send(n->clients, l, NULL);
			}
		} 

		if (linestack_store)
			linestack_insert_line(n->linestack, l, FROM_SERVER, n->state);
	} 

	return TRUE;
}

/**
 * Find a network by host name and port or name.
 * 
 * @param global Context to search in
 * @param hostname Hostname to search for
 * @param port Port to search from.
 * @param create Whether to create the network if it wasn't found.
 * @return the network found or created or NULL
 */
struct irc_network *find_network_by_hostname(struct global *global, 
										 const char *hostname, guint16 port, 
										 gboolean create)
{
	GList *gl;
	char *portname = g_strdup_printf("%d", port);

	g_assert(portname != NULL);
	g_assert(hostname != NULL);
	
	for (gl = global->networks; gl; gl = gl->next) {
		GList *sv;
		struct irc_network *n = gl->data;
		g_assert(n);

		if (n->info.name && !g_strcasecmp(n->info.name, hostname)) {
			g_free(portname);
			return n;
		}

		g_assert(n->config);
		if (n->config->type == NETWORK_TCP) 
		{
			for (sv = n->config->type_settings.tcp_servers; sv; sv = sv->next)
			{
				struct tcp_server_config *server = sv->data;
				struct servent *sv_serv = getservbyname(server->port, "tcp");

				if (!g_strcasecmp(server->host, hostname) &&
					(!g_strcasecmp(server->port, portname) 
					 || (sv_serv && htons(sv_serv->s_port) == port))) {
					g_free(portname);
					return n;
				}
			} 
		}

		if (n->info.name && !g_strcasecmp(n->info.name, hostname)) {
			g_free(portname);
			return n;
		}
	}

	/* Create a new server */
	if (create)
	{
		struct tcp_server_config *s = g_new0(struct tcp_server_config, 1);
		struct network_config *nc;
		struct irc_network *n;
		nc = network_config_init(global->config);

		nc->implicit = 1;
		nc->name = g_strdup(hostname);
		nc->type = NETWORK_TCP;
		s->host = g_strdup(hostname);
		s->port = portname;

		nc->type_settings.tcp_servers = g_list_append(nc->type_settings.tcp_servers, s);

		n = load_network(global, nc);
		network_set_log_fn(n, (network_log_fn)handle_network_log, n);
		return n;
	}

	g_free(portname);

	return NULL;
}

struct new_network_notify_data {
	new_network_notify_fn fn;
	void *data;
};

/**
 * Register a function that should be called when a new network is added.
 *
 * @param global Global loadparm.
 * @param fn Notification function
 * @param userdata Userdata
 */
void register_new_network_notify(struct global *global, new_network_notify_fn fn, void *userdata)
{
	struct new_network_notify_data *p = g_new0(struct new_network_notify_data, 1);
	p->fn = fn;
	p->data = userdata;
	global->new_network_notifiers = g_list_append(global->new_network_notifiers, p);
}

/**
 * Load a network from a configuration file specification.
 *
 * @param global Global context to use.
 * @param sc Network configuration to load form
 * @param process_from_server Function to handle lines received from server
 * @return A new network instance, already added to the global context.
 */
struct irc_network *load_network(struct global *global, struct network_config *sc)
{
	struct irc_network *net;

	if (global != NULL) {
		/* Don't connect to the same network twice */
		net = find_network(global->networks, sc->name);
		if (net) 
			return net;
	}
	
	net = irc_network_new(process_from_server, sc);

	net->global = global;

	if (global != NULL) {
		GList *gl;
		g_free(net->info.charset);
		if (net->global->config->client_charset != NULL) {
			net->info.charset = g_strdup(net->global->config->client_charset);
		}

		global->networks = g_list_append(global->networks, net);

		for (gl = global->new_network_notifiers; gl; gl = gl->next) {
			struct new_network_notify_data *p = gl->data;

			p->fn(net, p->data);
		}
	}

	return net;
}

/**
 * Load all the networks in a configuration file.
 *
 * @param global Global context to load networks into.
 * @param cfg Configuration to read from
 * @param fn Function to use for logging.
 * @return TRUE
 */
gboolean load_networks(struct global *global, struct ctrlproxy_config *cfg, 
					   network_log_fn fn)
{
	GList *gl;
	g_assert(cfg);
	for (gl = cfg->networks; gl; gl = gl->next)
	{
		struct network_config *nc = gl->data;
		struct irc_network *n;
		n = load_network(global, nc);
		if (n != NULL)
			network_set_log_fn(n, fn, n);
	}

	return TRUE;
}

/** 
 * Unload a network from a global context.
 *
 * @param s Network to unload.
 */
void unload_network(struct irc_network *s)
{
	GList *l;
	
	g_assert(s);
	l = s->clients;

	while(l) {
		struct irc_client *c = l->data;
		l = l->next;
		disconnect_client(c, "Server exiting");
	}

	if (s->global != NULL) {
		s->global->networks = g_list_remove(s->global->networks, s);
	}

	network_unref(s);
}
