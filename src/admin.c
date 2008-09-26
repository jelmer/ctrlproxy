/*
	ctrlproxy: A modular IRC proxy
	admin: module for remote administration. 

	(c) 2003-2007 Jelmer Vernooij <jelmer@nl.linux.org>

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
#include <string.h>
#include <sys/un.h>
#include "admin.h"
#include "help.h"
#include "irc.h"

help_t *help;

GList *admin_commands = NULL;
guint longest_command = 0;

static void add_network_helper(admin_handle h, const char *name);
static void del_network_helper(admin_handle h, const char *name);
static void list_networks_helper(admin_handle h);

#define ADMIN_USERNAME "ctrlproxy"
#define ADMIN_NICK "ctrlproxy"

/**
 * Determine the hostmask for the admin user.
 *
 * @param n Network for which to generate the hostmask.
 * @return Host mask, should be freed by the caller.
 */
static char *admin_hostmask(struct irc_network *n)
{
	return g_strdup_printf(ADMIN_NICK"!"ADMIN_USERNAME"@%s", n->name);
}

/**
 * Handle private message sent to the admin user.
 *
 * @param h Admin context handle.
 * @param data Text sent.
 */
static void privmsg_admin_out(admin_handle h, const char *data)
{
	struct irc_client *c = h->client;
	char *nick = c->state->me.nick;
	char *hostmask;

	hostmask = admin_hostmask(c->network);
	if (c->network->external_state != NULL) 
		nick = c->network->external_state->me.nick;
	client_send_args_ex(c, hostmask, "NOTICE", nick, data, NULL);

	g_free(hostmask);
}

/**
 * Sent data to the admin network channel.
 *
 * @param h Admin context handle
 * @param data Text to send.
 */
static void network_admin_out(admin_handle h, const char *data)
{
	struct irc_client *c = h->client;
	char *hostmask;

	hostmask = admin_hostmask(c->network);
	virtual_network_recv_args(c->network, hostmask, "PRIVMSG", ADMIN_CHANNEL, 
							  data, NULL);

	g_free(hostmask);
}

/**
 * Handles the 'help' command.
 * @param h Admin context handle
 * @param args Arguments
 * @param userdata User context data (ignored)
 */
static void cmd_help(admin_handle h, char **args, void *userdata)
{
	const char *s;

	s = help_get(help, args[1] != NULL?args[1]:"index");

	if (s == NULL) {
		if (args[1] == NULL)
			admin_out(h, "Sorry, help not available");
		else
			admin_out(h, "Sorry, no help for %s available", args[1]);
		return;
	}

	while (strncmp(s, "%\n", 2) != 0) {
		char *tmp;
		admin_out(h, "%s", tmp = g_strndup(s, strchr(s, '\n')-s));
		g_free(tmp);
			
		s = strchr(s, '\n')+1;
	}
}

/**
 * Return the client handle associated with an admin context.
 * @param h Admin context handle
 * @return Client, or NULL if no client is associated.
 */
struct irc_client *admin_get_client(admin_handle h)
{
	return h->client;
}

struct global *admin_get_global(admin_handle h)
{
	return h->global;
}

/**
 * Return the network handle associated with an admin context.
 * @param h Admin context handle
 * @return Network, or NULL if no network is associated.
 */
struct irc_network *admin_get_network(admin_handle h)
{
	return h->network;
}

/**
 * Send data to the user from the admin user.
 * @param h Admin context handle
 * @param fmt Format, printf-style
 * @param ... Format arguments
 */
void admin_out(admin_handle h, const char *fmt, ...)
{
	va_list ap;
	char *msg;
	va_start(ap, fmt);
	msg = g_strdup_vprintf(fmt, ap);
	va_end(ap);

	h->send_fn(h, msg);

	g_free(msg);
}

/**
 * Handles the 'ADDNETWORK' command.
 *
 * @param h Admin context handle.
 * @param args String arguments, argv-style.
 * @param userdata Optional user data, always NULL.
 */
static void cmd_add_network (admin_handle h, char **args, void *userdata)
{
	if (args[1] == NULL) {
		admin_out(h, "No name specified");
		return;
	}

	add_network_helper(h, args[1]);
}

static void cmd_del_network (admin_handle h, char **args, void *userdata)
{
	if (args[1] == NULL) {
		admin_out(h, "Not enough parameters");
		return;
	}

	del_network_helper(h, args[1]);
}

static void cmd_add_server (admin_handle h, char **args, void *userdata)
{
	struct irc_network *n;
	struct tcp_server_config *s;
	struct network_config *nc;
	char *t;

	if (!args[1] || !args[2]) {
		admin_out(h, "Not enough parameters");
		return;
	}

	n = find_network(admin_get_global(h)->networks, args[1]);

	if (!n) {
		admin_out(h, "No such network '%s'", args[1]);
		return;
	}

	nc = n->private_data;

	if (nc->type != NETWORK_TCP) {
		admin_out(h, "Not a TCP/IP network!");
		return;
	}

	s = g_new0(struct tcp_server_config, 1);

	s->host = g_strdup(args[2]);
	if ((t = strchr(s->host, ':'))) {
		*t = '\0';
		s->port = g_strdup(t+1);
	} else {
		s->port = g_strdup(DEFAULT_IRC_PORT);
	}
	s->ssl = FALSE;
	s->password = args[3]?g_strdup(args[3]):NULL;

	nc->type_settings.tcp.servers = g_list_append(nc->type_settings.tcp.servers, s);

	admin_out(h, "Server added to `%s'", args[1]);
}

static void cmd_connect_network (admin_handle h, char **args, void *userdata)
{
	struct irc_network *s;
	if (!args[1]) {
		 admin_out(h, "No network specified");
		 return;
	}

	s = find_network(admin_get_global(h)->networks, args[1]);

	if (!s) {
		admin_out(h, "No such network `%s'", args[1]);
		return;
	}

	switch (s->connection.state) {
		case NETWORK_CONNECTION_STATE_NOT_CONNECTED:
			admin_out(h, "Connecting to `%s'", args[1]);
			connect_network(s);
			break;
		case NETWORK_CONNECTION_STATE_RECONNECT_PENDING:
			admin_out(h, "Forcing reconnect to `%s'", args[1]);
			disconnect_network(s);
			network_select_next_server(s);
			connect_network(s);
			break;
		case NETWORK_CONNECTION_STATE_CONNECTED:
		case NETWORK_CONNECTION_STATE_CONNECTING:
		case NETWORK_CONNECTION_STATE_LOGIN_SENT:
			admin_out(h, "Connect to `%s' already in progress", args[1]);
			break;
		case NETWORK_CONNECTION_STATE_MOTD_RECVD:
			admin_out(h, "Already connected to `%s'", args[1]);
			break;
	}
}

static void cmd_disconnect_network (admin_handle h, char **args, void *userdata)
{
	struct irc_network *n;
	struct network_config *nc;

	if (args[1] != NULL) {
		n = find_network(admin_get_global(h)->networks, args[1]);
		if (!n) {
			admin_out(h, "Can't find active network with that name");
			return;
		}
	} else {
		n = admin_get_network(h);
		if (n == NULL) {
			admin_out(h, "Usage: DISCONNECT <network>");
			return;
		}
	}

	nc = n->private_data;

	if (n->connection.state == NETWORK_CONNECTION_STATE_NOT_CONNECTED) {
		admin_out(h, "Already disconnected from `%s'", n->name);
	} else if (nc->type == NETWORK_VIRTUAL && 
		n->connection.data.virtual.ops->not_disconnectable) {
		admin_out(h, "Built-in network `%s' can't be disconnected", 
				  n->name);
	} else {
		admin_out(h, "Disconnecting from `%s'", n->name);
		disconnect_network(n);
	}
}

static void cmd_next_server (admin_handle h, char **args, void *userdata) 
{
	struct irc_network *n;
	const char *name;


	if (args[1] != NULL) {
		name = args[1];
		n = find_network(admin_get_global(h)->networks, args[1]);
	} else {
		n = admin_get_network(h);
		if (n == NULL) {
			admin_out(h, "No current network");
			return;
		}
		name = n->name;
	}
	if (!n) {
		admin_out(h, "%s: Not connected", name);
	} else {
		admin_out(h, "%s: Reconnecting", name);
		disconnect_network(n);
		network_select_next_server(n);
		connect_network(n);
	}
}

static void cmd_save_config (admin_handle h, char **args, void *userdata)
{ 
	const char *adm_dir;
	global_update_config(admin_get_global(h));
	adm_dir = args[1]?args[1]:admin_get_global(h)->config->config_dir; 
	save_configuration(admin_get_global(h)->config, adm_dir);
	admin_out(h, "Configuration saved in %s", adm_dir);
}

static void add_network_helper(admin_handle h, const char *name)
{
	struct network_config *nc;
	struct irc_network *n;

	if (find_network(admin_get_global(h)->networks, name) != NULL) {
		admin_out(h, "Network with name `%s' already exists", name);
		return;
	}

	nc = network_config_init(admin_get_global(h)->config);
	g_free(nc->name); nc->name = g_strdup(name);
	n = load_network(admin_get_global(h), nc);

	admin_out(h, "Network `%s' added. Use ADDSERVER to add a server to this network.", name);
}

static void rename_network_helper(admin_handle h, const char *oldname, 
								  const char *newname)
{
	struct irc_network *n;

	n = find_network(admin_get_global(h)->networks, oldname);

	if (n == NULL) {
		admin_out(h, "Network with name `%s' does not exist", oldname);
		return;
	}

	if (find_network(admin_get_global(h)->networks, newname) != NULL) {
		admin_out(h, "Network with name `%s' already exists", newname);
		return;
	}
	
	g_free(n->name);
	n->name = g_strdup(newname);
}

static void del_network_helper(admin_handle h, const char *name)
{
	struct irc_network *n = find_network(admin_get_global(h)->networks, name);
	if (n == NULL) {
		admin_out(h, "No such network `%s'", name);
		return;
	}

	disconnect_network(n);

	unload_network(n);

	config_del_network(admin_get_global(h)->config, name);

	admin_out(h, "Network `%s' deleted", name);
}

static void list_networks_helper(admin_handle h)
{
	GList *gl;
	for (gl = admin_get_global(h)->networks; gl; gl = gl->next) {
		struct irc_network *n = gl->data;

		switch (n->connection.state) {
		case NETWORK_CONNECTION_STATE_CONNECTING:
			admin_out(h, "%s: Connect in progress", n->name);
			break;
		case NETWORK_CONNECTION_STATE_CONNECTED:
			admin_out(h, "%s: Connected, logging in", n->name);
			break;
		case NETWORK_CONNECTION_STATE_NOT_CONNECTED:
			if (n->connection.data.tcp.last_disconnect_reason)
				admin_out(h, "%s: Not connected: %s", n->name, 
						  n->connection.data.tcp.last_disconnect_reason);
			else
				admin_out(h, "%s: Not connected", n->name);
			break;
		case NETWORK_CONNECTION_STATE_RECONNECT_PENDING:
			admin_out(h, "%s: Reconnecting", n->name);
			break;
		case NETWORK_CONNECTION_STATE_LOGIN_SENT:
		case NETWORK_CONNECTION_STATE_MOTD_RECVD:
			admin_out(h, "%s: connected", n->name);
			break;
		}
	}
}

/* NETWORK LIST */
/* NETWORK ADD OFTC */
/* NETWORK DEL OFTC */
static void cmd_network(admin_handle h, char **args, void *userdata)
{
	if (args[1] == NULL)
		goto usage;

	if (!strcasecmp(args[1], "list")) {
		list_networks_helper(h);
	} else if (!strcasecmp(args[1], "add")) {
		if (args[2] == NULL) {
			admin_out(h, "Usage: network add <name>");
			return;
		}
		add_network_helper(h, args[2]);
	} else if (!strcasecmp(args[1], "rename")) {
		if (args[2] == NULL || args[3] == NULL) {
			admin_out(h, "Usage: network rename <oldname> <newname>");
			return;
		}
		rename_network_helper(h, args[2], args[3]);
	} else if (!strcasecmp(args[1], "del") || 
			   !strcasecmp(args[1], "delete") ||
			   !strcasecmp(args[1], "remove")) {
		if (args[2] == NULL) {
			admin_out(h, "Usage: network del <name>");
			return;
		}
		del_network_helper(h, args[2]);
		return;
	} else {
		goto usage;
	}

	return;
usage:
	admin_out(h, "Usage: network [list|add <name>|del <name>]");
}

static void cmd_list_networks(admin_handle h, char **args, void *userdata)
{
	list_networks_helper(h);
}

static void cmd_detach(admin_handle h, char **args, void *userdata)
{
	struct irc_client *c = admin_get_client(h);

	if (c == NULL) {
		admin_out(h, "No client to detach.");
		return;
	}

	client_disconnect(c, "Client exiting");
}

static void dump_joined_channels(admin_handle h, char **args, void *userdata)
{
	struct irc_network *n;
	GList *gl;

	if (args[1] != NULL) {
		n = find_network(admin_get_global(h)->networks, args[1]);
		if (n == NULL) {
			admin_out(h, "Can't find network '%s'", args[1]);
			return;
		}
	} else {
		n = admin_get_network(h);
		if (n == NULL) {
			admin_out(h, "No network specified and no current network.");
			return;
		}
	}

	if (!n->external_state) {
		admin_out(h, "Network '%s' not connected", n->name);
		return;
	}

	for (gl = n->external_state->channels; gl; gl = gl->next) {
		struct irc_channel_state *ch = (struct irc_channel_state *)gl->data;
		admin_out(h, "%s", ch->name);
	}
}

#ifdef DEBUG
static void cmd_abort(admin_handle h, char **args, void *userdata)
{
	abort();
}
#endif

static void cmd_die(admin_handle h, char **args, void *userdata)
{
	exit(0);
}

static GHashTable *markers = NULL;

static void cmd_backlog(admin_handle h, char **args, void *userdata)
{
	struct linestack_marker *lm;
	struct irc_network *n;

	if (admin_get_client(h) == NULL) {
		admin_out(h, "No client to send backlog to");
		return;
	}
	
	n = admin_get_network(h);

	if (n == NULL) {
		admin_out(h, "No network connected.");
		return;
	}

	lm = g_hash_table_lookup(markers, n);

	if (n->linestack == NULL) {
		admin_out(h, "No backlog available. Perhaps the connection to the network is down?");
		return;
	}

	if (!args[1] || strlen(args[1]) == 0) {
		admin_out(h, "Sending backlog for network '%s'", n->name);

		linestack_send(n->linestack, lm, NULL, admin_get_client(h),
					   TRUE, n->global->config->report_time != REPORT_TIME_NEVER,
					   n->global->config->report_time_offset);

		g_hash_table_replace(markers, n, linestack_get_marker(n->linestack));

		return;
	} 

	/* Backlog for specific nick/channel */
	admin_out(h, "Sending backlog for channel %s", args[1]);

	linestack_send_object(n->linestack, args[1], lm, NULL, 
						  admin_get_client(h), TRUE, 
						  n->global->config->report_time != REPORT_TIME_NEVER,
						  n->global->config->report_time_offset);

	g_hash_table_replace(markers, n, linestack_get_marker(n->linestack));
}

static char *linestack_get(admin_handle h)
{
	struct global *g = admin_get_global(h);

	return g_strdup(g->config->linestack_backend);
}

static gboolean linestack_set(admin_handle h, const char *value)
{
	struct global *g = admin_get_global(h);

	g_free(g->config->linestack_backend);
	/* FIXME: Check if this is a valid linestack backend name */
	g->config->linestack_backend = g_strdup(g->config->linestack_backend);

	return TRUE;
}

static char *logging_get(admin_handle h)
{
	struct global *g = admin_get_global(h);

	if (g->config->log_file == NULL)
		return g_strdup("none");

	if (g->config->log_file->is_irssi)
		return g_strdup("irssi");

	return g_strdup("custom");
}

static gboolean logging_set(admin_handle h, const char *value)
{
	/* FIXME: Implement */

	return FALSE;
}

static char *log_level_get(admin_handle h)
{
	extern enum log_level current_log_level;

	return g_strdup_printf("%d", current_log_level);
}

static gboolean log_level_set(admin_handle h, const char *value)
{
	extern enum log_level current_log_level;
	int x;

	if (value == NULL)
		return FALSE;
	
	x = atoi(value);
	if (x < 0 || x > 5) {
		admin_out(h, "Invalid log level %d", x);
		return FALSE;
	}

	current_log_level = x;
	admin_out(h, "Log level changed to %d", x);
	return TRUE;
}

static void cmd_charset(admin_handle h, char **args, void *userdata)
{
	struct irc_client *c;

	if (args[1] == NULL) {
		admin_out(h, "No charset specified");
		return;
	}

	c = admin_get_client(h);
	if (c == NULL) {
		admin_out(h, "The CHARSET command can only be used on client connections.");
		return;
	}

	if (!client_set_charset(c, args[1])) {
		admin_out(h, "Error setting charset: %s", args[1]);
	}
}

static void cmd_echo(admin_handle h, char **args, void *userdata)
{
	admin_out(h, "%s", args[1]);
}

static gint cmp_cmd(gconstpointer a, gconstpointer b)
{
	const struct admin_command *cmda = a, *cmdb = b;

	return g_strcasecmp(cmda->name, cmdb->name);
}

void register_admin_command(const struct admin_command *cmd)
{
	admin_commands = g_list_insert_sorted(admin_commands, g_memdup(cmd, sizeof(*cmd)), cmp_cmd);
	if (strlen(cmd->name) > longest_command) longest_command = strlen(cmd->name);
}

void unregister_admin_command(const struct admin_command *cmd)
{
	admin_commands = g_list_remove(admin_commands, cmd);
}

gboolean process_cmd(admin_handle h, const char *cmd)
{
	char **args = NULL;
	GList *gl;

	if (!cmd) {
		admin_out(h, "Please specify a command. Use the 'help' command to get a list of available commands");
		return TRUE;
	}

	args = g_strsplit(cmd, " ", 0);

	if (!args[0]) {
		admin_out(h, "Please specify a command. Use the 'help' command to get a list of available commands");
		return TRUE;
	}

	/* Ok, arguments are processed now. Execute the corresponding command */
	for (gl = admin_commands; gl; gl = gl->next) {
		struct admin_command *cmd = (struct admin_command *)gl->data;
		if (!g_strcasecmp(cmd->name, args[0])) {
			cmd->handler(h, args, cmd->userdata);
			g_strfreev(args);
			return TRUE;
		}
	}

	admin_out(h, "Can't find command '%s'. Type 'help' for a list of available commands. ", args[0]);

	g_strfreev(args);

	return TRUE;
}

gboolean admin_process_command(struct irc_client *c, struct irc_line *l, int cmdoffset)
{
	int i;
	char *tmp;
	gboolean ret;
	struct admin_handle ah;

	if (l->args[cmdoffset] == NULL) {
		client_send_response(c, ERR_NEEDMOREPARAMS, l->args[0], "Not enough parameters", NULL);
		return TRUE;
	}

	tmp = g_strdup(l->args[cmdoffset]);

	/* Add everything after l->args[cmdoffset] to tmp */
	for(i = cmdoffset+1; l->args[i]; i++) {
		char *oldtmp = tmp;
		tmp = g_strdup_printf("%s %s", oldtmp, l->args[i]);
		g_free(oldtmp);
	}

	ah.send_fn = privmsg_admin_out;
	ah.client = c;
	ah.network = c->network;
	ah.global = c->network->global;
	ret = process_cmd(&ah, tmp);

	g_free(tmp);

	return ret;
}

static gboolean admin_net_init(struct irc_network *n)
{
	char *hostmask;
	char *nicks;
	struct network_config *nc = n->private_data;

	hostmask = admin_hostmask(n);
	
	virtual_network_recv_args(n, n->external_state->me.hostmask, "JOIN", ADMIN_CHANNEL, NULL);
	virtual_network_recv_response(n, RPL_TOPIC, ADMIN_CHANNEL, 
		"CtrlProxy administration channel | Type `help' for more information",
							  NULL);
	nicks = g_strdup_printf("@ctrlproxy %s", nc->nick);

	virtual_network_recv_response(n, RPL_NAMREPLY, "=", ADMIN_CHANNEL, nicks, NULL);
	g_free(nicks);
	virtual_network_recv_response(n, RPL_ENDOFNAMES, ADMIN_CHANNEL, "End of /NAMES list.", NULL);

	g_free(hostmask);

	return TRUE;
}

static gboolean admin_to_server (struct irc_network *n, struct irc_client *c, const struct irc_line *l)
{
	if (!g_strcasecmp(l->args[0], "PRIVMSG") ||
		!g_strcasecmp(l->args[0], "NOTICE")) {
		struct admin_handle ah;

		if (g_strcasecmp(l->args[0], n->external_state->me.nick) == 0) {
			virtual_network_recv_args(n, n->external_state->me.hostmask, l->args[0], l->args[1], NULL);
			return TRUE;
		}

		if (g_strcasecmp(l->args[1], ADMIN_CHANNEL) && 
			g_strcasecmp(l->args[1], "ctrlproxy")) {
			virtual_network_recv_response(n, ERR_NOSUCHNICK, l->args[1], "No such nick/channel", NULL);
			return TRUE;
		}

		ah.send_fn = network_admin_out;
		ah.user_data = NULL;
		ah.client = c;
		ah.network = n;
		ah.global = n->global;

		return process_cmd(&ah, l->args[2]);
	} else if (!g_strcasecmp(l->args[0], "ISON")) {
		int i;
		char *tmp;
		GList *gl = NULL;

		if (l->args[1] == NULL) {
			virtual_network_recv_response(n, ERR_NEEDMOREPARAMS, l->args[0], "Not enough params", NULL);
			return TRUE;
		}

		for (i = 1; l->args[i]; i++) {
			if (!g_strcasecmp(l->args[i], "ctrlproxy") ||
				!g_strcasecmp(l->args[i], n->external_state->me.nick)) {
				gl = g_list_append(gl, l->args[i]);
			}
		}
		virtual_network_recv_response(n, RPL_ISON, tmp = list_make_string(gl), NULL);
		g_free(tmp);
		g_list_free(gl);
		return TRUE;
	} else if (!g_strcasecmp(l->args[0], "USERHOST")) {
		GList *gl = NULL;
		char *tmp;
		int i;

		if (l->args[1] == NULL) {
			virtual_network_recv_response(n, ERR_NEEDMOREPARAMS, l->args[0], "Not enough params", NULL);
			return TRUE;
		}

		for (i = 1; l->args[i]; i++) {
			if (!g_strcasecmp(l->args[i], "ctrlproxy")) {
				gl = g_list_append(gl, g_strdup_printf("%s=+%s@%s", l->args[i], ADMIN_USERNAME, get_my_hostname()));
			}
			if (!g_strcasecmp(l->args[i], n->external_state->me.nick)) {
				gl = g_list_append(gl, g_strdup_printf("%s=+%s@%s", l->args[i], n->external_state->me.username, n->external_state->me.hostname));
			}
		}

		virtual_network_recv_response(n, RPL_USERHOST, tmp = list_make_string(gl), NULL);
		g_free(tmp);
		while (gl) {
			g_free(gl->data);
			gl = g_list_remove(gl, gl->data);
		}
		return TRUE;
	} else if (!g_strcasecmp(l->args[0], "QUIT")) {
		return TRUE;
	} else if (!g_strcasecmp(l->args[0], "MODE")) {
		/* FIXME: Do something here ? */
		return TRUE;
	} else if (!g_strcasecmp(l->args[0], "WHO")) {
		if (!strcmp(l->args[1], ADMIN_CHANNEL) || 
			!strcmp(l->args[1], "ctrlproxy")) {
			virtual_network_recv_response(n, RPL_WHOREPLY, ADMIN_CHANNEL, 
									  "ctrlproxy",
									  get_my_hostname(),
									  get_my_hostname(),
									  "ctrlproxy",
									  "H",
									  "0 CtrlProxy user",
									  NULL);
		}
		if (!strcmp(l->args[1], ADMIN_CHANNEL) ||
			!strcmp(l->args[1], n->external_state->me.nick)) {
			char *fullname = g_strdup_printf("0 %s", n->external_state->me.fullname);
			virtual_network_recv_response(n, RPL_WHOREPLY, ADMIN_CHANNEL, 
									  n->external_state->me.username,
									  n->external_state->me.hostname,
									  get_my_hostname(),
									  n->external_state->me.nick,
									  "H",
									  fullname,
									  NULL);
			g_free(fullname);
		}

		virtual_network_recv_response(n, RPL_ENDOFWHO, l->args[1], "End of /WHO list.", NULL);

		return TRUE;
	} else if (!g_strcasecmp(l->args[0], "JOIN")) {
		if (strcmp(l->args[1], ADMIN_CHANNEL) != 0) {
			virtual_network_recv_response(n, ERR_NOSUCHCHANNEL, l->args[1], "No such channel", NULL);
		}
		return TRUE;
	} else if (!g_strcasecmp(l->args[0], "PART")) {
		if (strcmp(l->args[1], ADMIN_CHANNEL) != 0) {
			virtual_network_recv_response(n, ERR_NOTONCHANNEL, l->args[1], "You're not on that channel", NULL);
		} else {
			virtual_network_recv_args(n, n->external_state->me.hostmask, "PART", l->args[1], NULL);
			admin_net_init(n);
		}
		return TRUE;
	} else if (!g_strcasecmp(l->args[0], "WHOIS")) {
		/* FIXME: Send something sensible */
		virtual_network_recv_response(n, RPL_ENDOFWHOIS, l->args[1], 
									  "End of /WHOIS list.", NULL);
		return TRUE;
	} else if (!g_strcasecmp(l->args[0], "AWAY")) {
		if (l->args[1] != NULL && strcmp(l->args[1], "") != 0) {
			virtual_network_recv_response(n, RPL_NOWAWAY, "You are now marked as being away", NULL);
		} else {
			virtual_network_recv_response(n, RPL_UNAWAY, "You are no longer marked as being away", NULL);
		}
		return TRUE;
	} else {
		virtual_network_recv_response(n, ERR_UNKNOWNCOMMAND, l->args[0], "Unknown command", NULL);
		log_global(LOG_TRACE, "Unhandled command `%s' to admin network", 
				   l->args[0]);
		return TRUE;
	}
}

struct virtual_network_ops admin_network = {
	.name = "admin", 
	.init = admin_net_init, 
	.to_server = admin_to_server, 
	.not_disconnectable = TRUE,
};


void admin_log(enum log_level level, const struct irc_network *n, const struct irc_client *c, const char *data)
{
	extern struct global *my_global;
	struct irc_line *l;
	char *tmp, *hostmask;
	GList *gl;
	static gboolean entered = FALSE;

	if (!my_global || !my_global->config || 
		!my_global->config->admin_log) 
		return;

	if (level < LOG_INFO)
		return;

	if (entered)
		return; /* Prevent inifinite recursion.. */

	entered = TRUE;

	tmp = g_strdup_printf("%s%s%s%s%s%s", 
						  data, 
						  n?" (":"",
						  n?n->name:"", 
						  c?"/":"",
						  c?c->description:"",
						  n?")":"");

	for (gl = my_global->networks; gl; gl = gl->next) {
		struct irc_network *network = gl->data;

		if (network->connection.data.virtual.ops != &admin_network)
			continue;

		hostmask = admin_hostmask(network);
		l = irc_parse_line_args(hostmask, "PRIVMSG", ADMIN_CHANNEL, tmp, NULL); 
		g_free(hostmask);
		
		virtual_network_recv_line(network, l);

		free_line(l);
	}

	g_free(tmp);

	entered = FALSE;
}

static void cmd_start_listener(admin_handle h, char **args, void *userdata)
{
	char *b, *p;
	struct listener_config *cfg;
	struct irc_listener *l;

	if (!args[1]) {
		admin_out(h, "No port specified");
		return;
	}

	if (!args[2]) {
		admin_out(h, "No password specified");
		return;
	}

	cfg = g_new0(struct listener_config, 1);

	b = g_strdup(args[1]);
	if ((p = strchr(b, ':'))) {
		*p = '\0';
		p++;
		cfg->address = b;
		cfg->port = g_strdup(p);
	} else {
		cfg->port = g_strdup(b);
		cfg->address = NULL;
	}

	if (args[3]) {
		cfg->network = g_strdup(args[3]);
		if (find_network(admin_get_global(h)->networks, args[3]) == NULL) {
			admin_out(h, "No such network `%s`", args[3]);
			return;
		}
	}

	l = listener_init(admin_get_global(h), cfg);
	l->config->password = g_strdup(args[2]);
	l->global = admin_get_global(h);

	listener_start_tcp(l, l->config->address, l->config->port);
}

static void cmd_stop_listener(admin_handle h, char **args, void *userdata)
{
	GList *gl;
	char *b, *p;
	int i = 0;

	if (args[1] == NULL) {
		admin_out(h, "No port specified");
		return;
	}

	b = g_strdup(args[1]);
	if ((p = strchr(b, ':'))) {
		*p = '\0';
		p++;
	} else {
		p = b;
		b = NULL;
	}

	for (gl = admin_get_global(h)->listeners; gl; gl = gl->next) {
		struct irc_listener *l = gl->data;

		if (b && l->config->address == NULL)
			continue;

		if (b && l->config->address != NULL && strcmp(b, l->config->address) != 0)
			continue;

		if (strcmp(p, l->config->port) != 0)
			continue;

		listener_stop(l);
		free_listener(l);
		i++;
	}

	if (b) g_free(b); else g_free(p);

	admin_out(h, "%d listeners stopped", i);
}

static void cmd_list_listener(admin_handle h, char **args, void *userdata)
{
	GList *gl;

	for (gl = admin_get_global(h)->listeners; gl; gl = gl->next) {
		struct irc_listener *l = gl->data;

		admin_out(h, "%s:%s%s%s%s", l->config->address?l->config->address:"", l->config->port, 
				  l->network?" (":"", l->network?l->network->name:"", 
				  l->network?")":"");
	}
}

static gboolean motd_file_set(admin_handle h, const char *name)
{
	struct global *g = admin_get_global(h);

	if (!g_file_test(name, G_FILE_TEST_EXISTS)) {
		admin_out(h, "%s: file does not exist", name);
		return FALSE;
	}

	g_free(g->config->motd_file);
	g->config->motd_file = g_strdup(name);

	return TRUE;
}

static char *motd_file_get(admin_handle h)
{
	struct global *g = admin_get_global(h);

	return g_strdup(g->config->motd_file);
}

static gboolean interpret_boolean(admin_handle h, const char *value,
								  gboolean *ret)
{
	if (value == NULL) {
		admin_out(h, "Boolean settings can't be unset.");
		return FALSE;
	}

	if (!g_strcasecmp(value, "true")) {
		*ret = TRUE;
		return TRUE;
	} else if (!g_strcasecmp(value, "false")) {
		*ret = FALSE;
		return TRUE;
	}

	admin_out(h, "Invalid boolean value `%s'", value);
	return FALSE;
}

#define BOOL_SETTING(name) \
static char *name ## _get(admin_handle h) \
{ \
	struct global *g = admin_get_global(h); \
	return g_strdup(g->config->name?"true":"false"); \
} \
static gboolean name ## _set(admin_handle h, const char *value) \
{ \
	struct global *g = admin_get_global(h); \
	return interpret_boolean(h, value, &g->config->name); \
}

BOOL_SETTING(autosave)
BOOL_SETTING(admin_log)
BOOL_SETTING(learn_network_name)

static char *report_time_get(admin_handle h)
{
	struct global *g = admin_get_global(h);

	switch (g->config->report_time) {
	case REPORT_TIME_ALWAYS:
		return g_strdup("always");
	case REPORT_TIME_NEVER:
		return g_strdup("never");
	case REPORT_TIME_REPLICATION:
		return g_strdup("replication");
	}

	return g_strdup("UNKNOWN");
}

static gboolean report_time_set(admin_handle h, const char *value)
{
	struct global *g = admin_get_global(h);
	
	if (!g_strcasecmp(value, "never") || !g_strcasecmp(value, "false")) 
		g->config->report_time = REPORT_TIME_NEVER;
	else if (!g_strcasecmp(value, "always"))
		g->config->report_time = REPORT_TIME_ALWAYS;
	else if  (!g_strcasecmp(value, "replication") || 
			  !g_strcasecmp(value, "true"))
		g->config->report_time = REPORT_TIME_REPLICATION;
	else {
		return FALSE;
	}
	
	return TRUE;
}

static char *replication_get(admin_handle h)
{
	struct global *g = admin_get_global(h);

	return g_strdup(g->config->replication);
}

static gboolean replication_set(admin_handle h, const char *value)
{
	struct global *g = admin_get_global(h);

	if (repl_find_backend(value) == NULL) {
		admin_out(h, "No such replication backend `%s'", value);
		return FALSE;
	}

	g_free(g->config->replication);
	g->config->replication = g_strdup(value);

	return TRUE;
}

static char *admin_user_get(admin_handle h)
{
	struct global *g = admin_get_global(h);

	return g_strdup(g->config->admin_user);
}

static gboolean admin_user_set(admin_handle h, const char *value)
{
	struct global *g = admin_get_global(h);

	g_free(g->config->admin_user);
	if (strlen(value) == 0) {
		/* FIXME: Check whether value is a somewhat valid IRC nick? */
		g->config->admin_user = g_strdup(value);
	} else
		g->config->admin_user = NULL;

	return TRUE;
}

BOOL_SETTING(learn_nickserv)

static char *max_who_age_get(admin_handle h)
{
	struct global *g = admin_get_global(h);

	if (g->config->max_who_age == 0)
		return NULL;
	
	return g_strdup_printf("%d", g->config->max_who_age);
}

static gboolean max_who_age_set(admin_handle h, const char *value)
{
	struct global *g = admin_get_global(h);

	g->config->max_who_age = (value == NULL?0:atoi(value));

	return TRUE;
}

static char *auto_away_time_get(admin_handle h)
{
	struct global *g = admin_get_global(h);

	if (g->config->auto_away.max_idle_time == 0)
		return NULL;
	
	return g_strdup_printf("%d", g->config->auto_away.max_idle_time);
}

static gboolean auto_away_time_set(admin_handle h, const char *value)
{
	struct global *g = admin_get_global(h);

	g->config->auto_away.max_idle_time = (value == NULL?-1:atoi(value));

	/* FIXME: Restart auto-away */

	return TRUE;
}

static char *auto_away_enable_get(admin_handle h)
{
	struct global *g = admin_get_global(h);
	
	return g_strdup(g->config->auto_away.enabled?"true":"false");
}

static gboolean auto_away_enable_set(admin_handle h, const char *value)
{
	struct global *g = admin_get_global(h);

	return interpret_boolean(h, value, &g->config->auto_away.enabled);
}

static char *auto_away_message_get(admin_handle h)
{
	struct global *g = admin_get_global(h);
	
	return g_strdup(g->config->auto_away.message);
}

static gboolean auto_away_message_set(admin_handle h, const char *value)
{
	struct global *g = admin_get_global(h);

	g_free(g->config->auto_away.message);

	g->config->auto_away.message = g_strdup(value);

	return TRUE;
}

static char *auto_away_nick_get(admin_handle h)
{
	struct global *g = admin_get_global(h);
	
	return g_strdup(g->config->auto_away.nick);
}

static gboolean auto_away_nick_set(admin_handle h, const char *value)
{
	struct global *g = admin_get_global(h);

	g_free(g->config->auto_away.nick);

	g->config->auto_away.nick = g_strdup(value);

	return TRUE;
}

static char *report_time_offset_get(admin_handle h)
{
	struct global *g = admin_get_global(h);

	if (g->config->report_time_offset == 0)
		return NULL;
	
	return g_strdup_printf("%d", g->config->report_time_offset);
}

static gboolean report_time_offset_set(admin_handle h, const char *value)
{
	struct global *g = admin_get_global(h);

	g->config->report_time_offset = (value == NULL?0:atoi(value));

	return TRUE;
}

static char *port_get(admin_handle h)
{
	GList *gl;
	struct global *g = admin_get_global(h);

	for (gl = g->config->listeners; gl; gl = gl->next) {
		struct listener_config *l = gl->data;

		if (l->is_default)
			return g_strdup_printf("%s", l->port);
	}

	return NULL;
}

static gboolean port_set(admin_handle h, const char *value)
{
	GList *gl;
	struct global *g = admin_get_global(h);

	/* FIXME: What if there is no default listener ? */
	/* FIXME: Check that value is a valid service name or port number */

	for (gl = g->config->listeners; gl; gl = gl->next) {
		struct listener_config *l = gl->data;

		if (l->is_default) {
			l->port = g_strdup(value);
			return TRUE;
		}
	}

	return FALSE;
}

static char *bind_get(admin_handle h)
{
	GList *gl;
	struct global *g = admin_get_global(h);

	for (gl = g->config->listeners; gl; gl = gl->next) {
		struct listener_config *l = gl->data;

		if (l->is_default)
			return g_strdup(l->address);
	}

	return NULL;
}

static gboolean bind_set(admin_handle h, const char *value)
{
	GList *gl;
	struct global *g = admin_get_global(h);

	/* FIXME: What if there is no default listener ? */
	/* FIXME: Check that value is a valid service name or port number */

	for (gl = g->config->listeners; gl; gl = gl->next) {
		struct listener_config *l = gl->data;

		if (l->is_default) {
			l->address = g_strdup(value);
			return TRUE;
		}
	}

	return FALSE;
}

static char *password_get(admin_handle h)
{
	GList *gl;
	struct global *g = admin_get_global(h);

	for (gl = g->config->listeners; gl; gl = gl->next) {
		struct listener_config *l = gl->data;

		if (l->is_default)
			return g_strdup_printf("%s", l->password);
	}

	return NULL;
}

static gboolean password_set(admin_handle h, const char *value)
{
	GList *gl;
	struct global *g = admin_get_global(h);

	/* FIXME: What if there is no default listener ? */
	/* FIXME: Check that value is a valid service name or port number */

	for (gl = g->config->listeners; gl; gl = gl->next) {
		struct listener_config *l = gl->data;

		if (l->is_default) {
			l->password = g_strdup(value);
			return TRUE;
		}
	}

	return FALSE;
}

static char *default_network_get(admin_handle h)
{
	GList *gl;
	struct global *g = admin_get_global(h);

	for (gl = g->config->listeners; gl; gl = gl->next) {
		struct listener_config *l = gl->data;

		if (l->is_default)
			return g_strdup(l->network);
	}

	return NULL;
}

static gboolean default_network_set(admin_handle h, const char *value)
{
	GList *gl;
	struct global *g = admin_get_global(h);

	/* FIXME: What if there is no default listener ? */
	/* FIXME: Check that value is a valid service name or port number */

	for (gl = g->config->listeners; gl; gl = gl->next) {
		struct listener_config *l = gl->data;

		if (l->is_default) {
			l->network = g_strdup(value);
			return TRUE;
		}
	}

	return FALSE;
}

static char *default_client_charset_get(admin_handle h)
{
	struct global *g = admin_get_global(h);

	return g_strdup(g->config->client_charset);
}

static gboolean default_client_charset_set(admin_handle h, const char *value)
{
	struct global *g = admin_get_global(h);

	g->config->client_charset = g_strdup(value);

	return TRUE;
}


/**
 * Table of administration settings that can be
 * viewed and changed using the SET command.
 */
static struct admin_setting {
	const char *name;
	char *(*get) (admin_handle h);
	gboolean (*set) (admin_handle h, const char *value);
} settings[] = {
	{ "admin-log", admin_log_get, admin_log_set },
	{ "admin-user", admin_user_get, admin_user_set },
	{ "auto-away-enable", auto_away_enable_get, auto_away_enable_set },
	{ "auto-away-message", auto_away_message_get, auto_away_message_set },
	{ "auto-away-nick", auto_away_nick_get, auto_away_nick_set },
	{ "auto-away-time", auto_away_time_get, auto_away_time_set },
	{ "autosave", autosave_get, autosave_set },
	{ "bind", bind_get, bind_set },
	{ "default-client-charset", default_client_charset_get, default_client_charset_set },
	{ "default-network", default_network_get, default_network_set },
	{ "learn-network-name", learn_network_name_get, learn_network_name_set },
	{ "learn-nickserv", learn_nickserv_get, learn_nickserv_set },
	{ "linestack", linestack_get, linestack_set },
	{ "log_level", log_level_get, log_level_set },
	{ "logging", logging_get, logging_set },
	{ "max_who_age", max_who_age_get, max_who_age_set },
	{ "motd-file", motd_file_get, motd_file_set },
	{ "password", password_get, password_set },
	{ "port", port_get, port_set },
	{ "report-time", report_time_get, report_time_set },
	{ "report-time-offset", report_time_offset_get, report_time_offset_set },
	{ "replication", replication_get, replication_set },
	{ NULL, NULL, NULL }
};

static void cmd_unset(admin_handle h, char **args, void *userdata)
{
	int i;
	if (args[1] == NULL) {
		admin_out(h, "Usage: unset <setting>");
		return;
	}

	for (i = 0; settings[i].name; i++) {
		if (!strcasecmp(settings[i].name, args[1])) {
			settings[i].set(h, NULL);
		}
	}
}

static void cmd_set(admin_handle h, char **args, void *userdata)
{
	int i;
	char *tmp;

	if (args[1] == NULL) {
		for (i = 0; settings[i].name != NULL; i++) {
			tmp = settings[i].get(h);
			if (tmp == NULL)
				admin_out(h, "%s is not set", settings[i]);
			else
				admin_out(h, "%s = %s", settings[i].name, tmp);
			g_free(tmp);
		}
	} else {
		for (i = 0; settings[i].name; i++) {
			if (!strcasecmp(settings[i].name, args[1])) {
				if (args[2] != NULL) {
					settings[i].set(h, args[2]);
				} else {
					tmp = settings[i].get(h);
					admin_out(h, "%s", tmp);
					g_free(tmp);
				}
				return;
			}
		}

		admin_out(h, "Unknown setting `%s'", args[1]);
	}
}

const static struct admin_command builtin_commands[] = {
	/* Provided for backwards compatibility */
	{ "ADDNETWORK", cmd_add_network },
	{ "DELNETWORK", cmd_del_network },
	{ "LISTNETWORKS", cmd_list_networks },

	/* Commands */
	{ "SET", cmd_set },
	{ "UNSET", cmd_unset },

	{ "ADDSERVER", cmd_add_server },
	{ "BACKLOG", cmd_backlog},
	{ "CONNECT", cmd_connect_network },
	{ "DISCONNECT", cmd_disconnect_network },
	{ "ECHO", cmd_echo },
	{ "NEXTSERVER", cmd_next_server },
	{ "CHARSET", cmd_charset },
	{ "DIE", cmd_die },
	{ "NETWORK", cmd_network },
	{ "SAVECONFIG", cmd_save_config },
	{ "DETACH", cmd_detach },
	{ "HELP", cmd_help },
	{ "DUMPJOINEDCHANNELS", dump_joined_channels },
	{ "STARTLISTENER", cmd_start_listener },
	{ "STOPLISTENER", cmd_stop_listener },
	{ "LISTLISTENER", cmd_list_listener },
#ifdef DEBUG
	{ "ABORT", cmd_abort },
#endif
	{ NULL }
};

void init_admin(void) 
{
	int i;
	for(i = 0; builtin_commands[i].name; i++) {
		register_admin_command(&builtin_commands[i]);
	}

	register_virtual_network(&admin_network);

	markers = g_hash_table_new_full(NULL, NULL, NULL, (GDestroyNotify)linestack_free_marker);
}

static void iochannel_admin_out(admin_handle h, const char *data)
{
	gsize bytes_written;
	GError *error = NULL;
	GIOStatus status;

	status = g_io_channel_write_chars(h->user_data, data, -1, &bytes_written, &error);

	status = g_io_channel_write_chars(h->user_data, "\n", -1, &bytes_written, &error);

	status = g_io_channel_flush(h->user_data, &error);
}

static gboolean handle_client_data(GIOChannel *channel, 
								  GIOCondition condition, void *_global)
{
	char *raw;
	GError *error = NULL;
	GIOStatus status;
	struct admin_handle ah;
	gsize eol;

	ah.global = _global;
	ah.client = NULL;
	ah.user_data = channel;
	ah.send_fn = iochannel_admin_out;

	if (condition & G_IO_IN) {
		status = g_io_channel_read_line(channel, &raw, NULL, &eol, &error);
		if (status == G_IO_STATUS_NORMAL) {
			raw[eol] = '\0';
			process_cmd(&ah, raw);
			g_free(raw);
		}
	}

	if (condition & G_IO_HUP) {
		return FALSE;
	}

	if (condition & G_IO_ERR) {
		log_global(LOG_WARNING, "Error from admin client");
		return FALSE;
	}

	return TRUE;
}

static gboolean handle_new_client(GIOChannel *c_server, 
								  GIOCondition condition, void *_global)
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
	
	g_io_add_watch(c, G_IO_IN | G_IO_ERR | G_IO_HUP, handle_client_data, _global);

	g_io_channel_unref(c);

	return TRUE;
}

gboolean start_admin_socket(struct global *global)
{
	int sock;
	struct sockaddr_un un;

	sock = socket(PF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) {
		log_global(LOG_ERROR, "error creating unix socket: %s", strerror(errno));
		return FALSE;
	}
	
	un.sun_family = AF_UNIX;
	strncpy(un.sun_path, global->config->admin_socket, sizeof(un.sun_path));
	unlink(un.sun_path);

	if (bind(sock, (struct sockaddr *)&un, sizeof(un)) < 0) {
		log_global(LOG_ERROR, "unable to bind to %s: %s", un.sun_path, strerror(errno));
		return FALSE;
	}
	
	if (listen(sock, 5) < 0) {
		log_global(LOG_ERROR, "error listening on socket: %s", strerror(errno));
		return FALSE;
	}

	global->admin_incoming = g_io_channel_unix_new(sock);

	if (!global->admin_incoming) {
		log_global(LOG_ERROR, "Unable to create GIOChannel for unix server socket");
		return FALSE;
	}

	g_io_channel_set_close_on_unref(global->admin_incoming, TRUE);

	g_io_channel_set_encoding(global->admin_incoming, NULL, NULL);
	global->admin_incoming_id = g_io_add_watch(global->admin_incoming, G_IO_IN, handle_new_client, global);
	g_io_channel_unref(global->admin_incoming);

	return TRUE;
}

gboolean stop_admin_socket(struct global *global)
{
	if (global->admin_incoming_id > 0)
		g_source_remove(global->admin_incoming_id);
	unlink(global->config->admin_socket);
	return TRUE;
}
