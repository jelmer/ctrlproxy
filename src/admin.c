/*
	ctrlproxy: A modular IRC proxy
	admin: module for remote administration. 

	(c) 2003-2007 Jelmer Vernooij <jelmer@nl.linux.org>

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
#include <string.h>
#include "admin.h"
#include "help.h"
#include "irc.h"

help_t *help;

#define ADMIN_CHANNEL "#ctrlproxy"

GList *admin_commands = NULL;
guint longest_command = 0;

static void privmsg_admin_out(admin_handle h, const char *data)
{
	struct client *c = h->client;
	char *nick = c->nick;
	char *hostmask;

	hostmask = g_strdup_printf("ctrlproxy!ctrlproxy@%s", c->network->info.name);
	if (c->network->state) nick = c->network->state->me.nick;
	client_send_args_ex(c, hostmask, "NOTICE", nick, data, NULL);

	g_free(hostmask);
}

static void network_admin_out(admin_handle h, const char *data)
{
	struct client *c = h->client;
	char *hostmask;

	hostmask = g_strdup_printf("ctrlproxy!ctrlproxy@%s", c->network->info.name);
	virtual_network_recv_args(c->network, hostmask, "PRIVMSG", ADMIN_CHANNEL, 
							  data, NULL);

	g_free(hostmask);
}

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

struct client *admin_get_client(admin_handle h)
{
	return h->client;
}

struct global *admin_get_global(admin_handle h)
{
	return h->global;
}

struct network *admin_get_network(admin_handle h)
{
	return h->network;
}

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

static void add_network (admin_handle h, char **args, void *userdata)
{
	struct network_config *nc;

	if (args[1] == NULL) {
		admin_out(h, "No name specified");
		return;
	}

	if (find_network(admin_get_global(h), args[1]) != NULL) {
		admin_out(h, "Network with name `%s' already exists", args[1]);
		return;
	}

	nc = network_config_init(admin_get_global(h)->config);
	g_free(nc->name); nc->name = g_strdup(args[1]);
	load_network(admin_get_global(h), nc);

	admin_out(h, "Network `%s' added. Use ADDSERVER to add a server to this network.", args[1]);
}

static void del_network (admin_handle h, char **args, void *userdata)
{
	struct network *n;

	if (args[1] == NULL) {
		admin_out(h, "Not enough parameters");
		return;
	}

	n = find_network(admin_get_global(h), args[1]);
	if (n == NULL) {
		admin_out(h, "No such network `%s'", args[1]);
		return;
	}

	disconnect_network(n);

	unload_network(n);

	admin_out(h, "Network `%s' deleted", args[1]);
}

static void add_server (admin_handle h, char **args, void *userdata)
{
	struct network *n;
	struct tcp_server_config *s;
	char *t;

	if(!args[1] || !args[2]) {
		admin_out(h, "Not enough parameters");
		return;
	}

	n = find_network(admin_get_global(h), args[1]);

	if (!n) {
		admin_out(h, "No such network '%s'", args[1]);
		return;
	}

	if (n->config->type != NETWORK_TCP) {
		admin_out(h, "Not a TCP/IP network!");
		return;
	}

	s = g_new0(struct tcp_server_config, 1);

	s->host = g_strdup(args[2]);
	if ((t = strchr(s->host, ':'))) {
		*t = '\0';
		s->port = g_strdup(t+1);
	} else {
		s->port = g_strdup("6667");
	}
	s->ssl = FALSE;
	s->password = args[3]?g_strdup(args[3]):NULL;

	n->config->type_settings.tcp_servers = g_list_append(n->config->type_settings.tcp_servers, s);

	admin_out(h, "Server added to `%s'", args[1]);
}

static void com_connect_network (admin_handle h, char **args, void *userdata)
{
	struct network *s;
	if(!args[1]) {
		 admin_out(h, "No network specified");
		 return;
	}

	s = find_network(admin_get_global(h), args[1]);

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
		case NETWORK_CONNECTION_STATE_LOGIN_SENT:
			admin_out(h, "Connect to `%s' already in progress", args[1]);
			break;
		case NETWORK_CONNECTION_STATE_MOTD_RECVD:
			admin_out(h, "Already connected to `%s'", args[1]);
			break;
	}
}

static void com_disconnect_network (admin_handle h, char **args, void *userdata)
{
	struct network *n;

	n = admin_get_network(h);

	if (args[1] != NULL) {
		n = find_network(admin_get_global(h), args[1]);
		if(!n) {
			admin_out(h, "Can't find active network with that name");
			return;
		}
	}

	if (n->connection.state == NETWORK_CONNECTION_STATE_NOT_CONNECTED) {
		admin_out(h, "Already disconnected from `%s'", args[1]);
	} else {
		admin_out(h, "Disconnecting from `%s'", args[1]);
		disconnect_network(n);
	}
}

static void com_next_server (admin_handle h, char **args, void *userdata) 
{
	struct network *n;
	const char *name;


	if(args[1] != NULL) {
		name = args[1];
		n = find_network(admin_get_global(h), args[1]);
	} else {
		n = admin_get_network(h);
		name = n->info.name;
	}
	if(!n) {
		admin_out(h, "%s: Not connected", name);
	} else {
		admin_out(h, "%s: Reconnecting", name);
		disconnect_network(n);
		network_select_next_server(n);
		connect_network(n);
	}
}

static void com_save_config (admin_handle h, char **args, void *userdata)
{ 
	const char *adm_dir;
	global_update_config(admin_get_global(h));
	adm_dir = args[1]?args[1]:admin_get_global(h)->config->config_dir; 
	save_configuration(admin_get_global(h)->config, adm_dir);
	admin_out(h, "Configuration saved in %s", adm_dir);
}



static void list_networks(admin_handle h, char **args, void *userdata)
{
	GList *gl;
	for (gl = admin_get_global(h)->networks; gl; gl = gl->next) {
		struct network *n = gl->data;

		switch (n->connection.state) {
		case NETWORK_CONNECTION_STATE_NOT_CONNECTED:
			if (n->connection.data.tcp.last_disconnect_reason)
				admin_out(h, "%s: Not connected: %s", n->info.name, 
						  n->connection.data.tcp.last_disconnect_reason);
			else
				admin_out(h, "%s: Not connected", n->info.name);
			break;
		case NETWORK_CONNECTION_STATE_RECONNECT_PENDING:
			admin_out(h, "%s: Reconnecting", n->info.name);
			break;
		case NETWORK_CONNECTION_STATE_LOGIN_SENT:
		case NETWORK_CONNECTION_STATE_MOTD_RECVD:
			admin_out(h, "%s: connected", n->info.name);
			break;
		}
	}
}

static void detach_client(admin_handle h, char **args, void *userdata)
{
	struct client *c = admin_get_client(h);

	disconnect_client(c, "Client exiting");
}

static void dump_joined_channels(admin_handle h, char **args, void *userdata)
{
	struct network *n;
	GList *gl;

	if (args[1] != NULL) {
		n = find_network(admin_get_global(h), args[1]);
		if(n == NULL) {
			admin_out(h, "Can't find network '%s'", args[1]);
			return;
		}
	} else {
		n = admin_get_network(h);
	}

	if (!n->state) {
		admin_out(h, "Network '%s' not connected", n->info.name);
		return;
	}

	for (gl = n->state->channels; gl; gl = gl->next) {
		struct channel_state *ch = (struct channel_state *)gl->data;
		admin_out(h, "%s", ch->name);
	}
}

#ifdef DEBUG
static void do_abort(admin_handle h, char **args, void *userdata)
{
	abort();
}
#endif

static void handle_die(admin_handle h, char **args, void *userdata)
{
	exit(0);
}

static GHashTable *markers = NULL;

static void repl_command(admin_handle h, char **args, void *userdata)
{
	struct linestack_marker *lm;
	struct network *n;
	
	n = admin_get_network(h);

	lm = g_hash_table_lookup(markers, n);

	if (n->linestack == NULL) {
		admin_out(h, "No backlog available. Perhaps the connection to the network is down?");
		return;
	}

	if (!args[1] || strlen(args[1]) == 0) {
		admin_out(h, "Sending backlog for network '%s'", n->info.name);

		linestack_send(n->linestack, lm, NULL, admin_get_client(h),
					   TRUE, n->global->config->report_time);

		g_hash_table_replace(markers, n, linestack_get_marker(n->linestack));

		return;
	} 

	/* Backlog for specific nick/channel */
	admin_out(h, "Sending backlog for channel %s", args[1]);

	linestack_send_object(n->linestack, args[1], lm, NULL, 
						  admin_get_client(h), TRUE, 
						  n->global->config->report_time);

	g_hash_table_replace(markers, n, linestack_get_marker(n->linestack));
}

static void cmd_log_level(admin_handle h, char **args, void *userdata)
{
	extern enum log_level current_log_level;
	
	if (args[1] == NULL) 
		admin_out(h, "Current log level: %d", current_log_level);
	else {
		int x = atoi(args[1]);
		if (x < 0 || x > 5) 
			admin_out(h, "Invalid log level %d", x);
		else { 
			current_log_level = x;
			admin_out(h, "Log level changed to %d", x);
		}
	}
}

static void handle_charset(admin_handle h, char **args, void *userdata)
{
	struct client *c;

	if (args[1] == NULL) {
		admin_out(h, "No charset specified");
		return;
	}

	c = admin_get_client(h);

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
		if(!g_strcasecmp(cmd->name, args[0])) {
			cmd->handler(h, args, cmd->userdata);
			g_strfreev(args);
			return TRUE;
		}
	}

	admin_out(h, "Can't find command '%s'. Type 'help' for a list of available commands. ", args[0]);

	g_strfreev(args);

	return TRUE;
}

gboolean admin_process_command(struct client *c, struct line *l, int cmdoffset)
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

static gboolean admin_net_init(struct network *n)
{
	char *hostmask;
	char *nicks;

	hostmask = g_strdup_printf("ctrlproxy!ctrlproxy@%s", n->info.name);
	
	virtual_network_recv_args(n, n->state->me.hostmask, "JOIN", ADMIN_CHANNEL, NULL);
	virtual_network_recv_response(n, RPL_TOPIC, ADMIN_CHANNEL, 
		"CtrlProxy administration channel | Type `help' for more information",
							  NULL);
	nicks = g_strdup_printf("@ctrlproxy %s", n->config->nick);

	virtual_network_recv_response(n, RPL_NAMREPLY, "=", ADMIN_CHANNEL, nicks, NULL);
	g_free(nicks);
	virtual_network_recv_response(n, RPL_ENDOFNAMES, ADMIN_CHANNEL, "End of /NAMES list.", NULL);

	g_free(hostmask);

	return TRUE;
}

static gboolean admin_to_server (struct network *n, struct client *c, const struct line *l)
{
	if (!g_strcasecmp(l->args[0], "PRIVMSG") ||
		!g_strcasecmp(l->args[0], "NOTICE")) {
		struct admin_handle ah;

		if (g_strcasecmp(l->args[0], n->state->me.nick) == 0) {
			virtual_network_recv_args(n, n->state->me.hostmask, l->args[0], l->args[1], NULL);
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
				!g_strcasecmp(l->args[i], n->state->me.nick)) {
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
				gl = g_list_append(gl, g_strdup_printf("%s=+%s", l->args[i], get_my_hostname()));
			}
			if (!g_strcasecmp(l->args[i], n->state->me.nick)) {
				gl = g_list_append(gl, g_strdup_printf("%s=+%s", l->args[i], n->state->me.hostname));
			}
		}

		virtual_network_recv_response(n, RPL_ISON, tmp = list_make_string(gl), NULL);
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
			!strcmp(l->args[1], n->state->me.nick)) {
			char *fullname = g_strdup_printf("0 %s", n->state->me.fullname);
			virtual_network_recv_response(n, RPL_WHOREPLY, ADMIN_CHANNEL, 
									  n->state->me.username,
									  n->state->me.hostname,
									  get_my_hostname(),
									  n->state->me.nick,
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
			virtual_network_recv_args(n, n->state->me.hostmask, "PART", l->args[1], NULL);
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
	"admin", admin_net_init, admin_to_server, NULL
};


void admin_log(enum log_level level, const struct network *n, const struct client *c, const char *data)
{
	extern struct global *my_global;
	struct line *l;
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
						  n?n->info.name:"", 
						  c?"/":"",
						  c?c->description:"",
						  n?")":"");

	for (gl = my_global->networks; gl; gl = gl->next) {
		struct network *network = gl->data;

		if (network->connection.data.virtual.ops != &admin_network)
			continue;

		hostmask = g_strdup_printf("ctrlproxy!ctrlproxy@%s", network->info.name);
		l = irc_parse_line_args(hostmask, "PRIVMSG", ADMIN_CHANNEL, tmp, NULL); 
		g_free(hostmask);
		
		virtual_network_recv_line(network, l);

		free_line(l);
	}

	g_free(tmp);

	entered = FALSE;
}

const static struct admin_command builtin_commands[] = {
	{ "ADDNETWORK", add_network },
	{ "ADDSERVER", add_server },
	{ "BACKLOG", repl_command },
	{ "CONNECT", com_connect_network },
	{ "DELNETWORK", del_network },
	{ "ECHO", cmd_echo },
	{ "LOG_LEVEL", cmd_log_level },
	{ "NEXTSERVER", com_next_server },
	{ "CHARSET", handle_charset },
	{ "DIE", handle_die },
	{ "DISCONNECT", com_disconnect_network },
	{ "LISTNETWORKS", list_networks },
	{ "SAVECONFIG", com_save_config },
	{ "DETACH", detach_client },
	{ "HELP", cmd_help },
	{ "DUMPJOINEDCHANNELS", dump_joined_channels },
#ifdef DEBUG
	{ "ABORT", do_abort },
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
