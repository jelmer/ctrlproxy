/*
	ctrlproxy: A modular IRC proxy
	admin: module for remote administration. 

	(c) 2003-2006 Jelmer Vernooij <jelmer@nl.linux.org>

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

#include "ctrlproxy.h"
#include <string.h>
#include "admin.h"

#define ADMIN_CHANNEL "#ctrlproxy"

static GList *commands = NULL;
static guint longest_command = 0;

void admin_out(struct client *c, const char *fmt, ...)
{
	va_list ap;
	char *msg;
	char *hostmask;
	va_start(ap, fmt);
	msg = g_strdup_vprintf(fmt, ap);
	va_end(ap);

	hostmask = g_strdup_printf("ctrlproxy!ctrlproxy@%s", c->network->name);
	if (c->network->config->type == NETWORK_VIRTUAL && 
		!strcmp(c->network->connection.data.virtual.ops->name, "admin")) {
		virtual_network_recv_args(c->network, hostmask, "PRIVMSG", ADMIN_CHANNEL, msg, NULL);
	} else {
		char *nick = c->nick;
		if (c->network->state) nick = c->network->state->me.nick;
		client_send_args_ex(c, hostmask, "NOTICE", nick, msg, NULL);
	}
	g_free(hostmask);

	g_free(msg);
}

static void add_network (struct client *c, char **args, void *userdata)
{
	struct network_config *nc;

	if (args[1] == NULL) {
		admin_out(c, "No name specified");
		return;
	}

	nc = network_config_init(c->network->global->config);
	g_free(nc->name); nc->name = g_strdup(args[1]);
	load_network(c->network->global, nc);

	admin_out(c, "Network `%s' added. Use ADDSERVER to add a server to this network.", args[1]);
}

static void del_network (struct client *c, char **args, void *userdata)
{
	struct network *n;

	if (args[1] == NULL) {
		admin_out(c, "Not enough parameters");
		return;
	}

	n = find_network(c->network->global, args[1]);
	if (n == NULL) {
		admin_out(c, "No such network `%s'", args[1]);
		return;
	}

	disconnect_network(n);

	admin_out(c, "Network `%s' deleted", args[1]);
}

static void add_server (struct client *c, char **args, void *userdata)
{
	struct network *n;
	struct tcp_server_config *s;
	char *t;

	if(!args[1] || !args[2]) {
		admin_out(c, "Not enough parameters");
		return;
	}

	n = find_network(c->network->global, args[1]);

	if (!n) {
		admin_out(c, "No such network '%s'", args[1]);
		return;
	}

	if (n->config->type != NETWORK_TCP) {
		admin_out(c, "Not a TCP/IP network!");
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

	admin_out(c, "Server added to `%s'", args[1]);
}

static void com_connect_network (struct client *c, char **args, void *userdata)
{
	struct network *s;
	if(!args[1]) {
		 admin_out(c, "No network specified");
		 return;
	}

	s = find_network(c->network->global, args[1]);

	if (!s) {
		admin_out(c, "No such network `%s'", args[1]);
		return;
	}

	switch (s->connection.state) {
		case NETWORK_CONNECTION_STATE_NOT_CONNECTED:
			admin_out(c, "Connecting to `%s'", args[1]);
			connect_network(s);
			break;
		case NETWORK_CONNECTION_STATE_RECONNECT_PENDING:
			admin_out(c, "Forcing reconnect to `%s'", args[1]);
			disconnect_network(s);
			network_select_next_server(s);
			connect_network(s);
			break;
		case NETWORK_CONNECTION_STATE_CONNECTED:
			admin_out(c, "Already connected to `%s'", args[1]);
			break;
		case NETWORK_CONNECTION_STATE_LOGIN_SENT:
		case NETWORK_CONNECTION_STATE_MOTD_RECVD:
			admin_out(c, "Connect to `%s' already in progress", args[1]);
			break;
	}
}

static void com_disconnect_network (struct client *c, char **args, void *userdata)
{
	struct network *n;
	if(!args[1])n = c->network;
	else {
		n = find_network(c->network->global, args[1]);
		if(!n) {
			admin_out(c, "Can't find active network with that name");
			return;
		}
	}

	if (n->connection.state == NETWORK_CONNECTION_STATE_NOT_CONNECTED) {
		admin_out(c, "Already disconnected from `%s'", args[1]);
	} else {
		admin_out(c, "Disconnecting from `%s'", args[1]);
		disconnect_network(n);
	}
}

static void com_next_server (struct client *c, char **args, void *userdata) {
	struct network *n;
	const char *name;
	if(args[1] != NULL) {
		name = args[1];
		n = find_network(c->network->global, args[1]);
	} else {
		n = c->network;
		name = n->name;
	}
	if(!n) {
		admin_out(c, "%s: Not connected", name);
	} else {
		admin_out(c, "%s: Reconnecting", name);
		disconnect_network(n);
		network_select_next_server(n);
		connect_network(n);
	}
}

static void list_modules (struct client *c, char **args, void *userdata)
{
	GList *g;
	
	for (g = get_plugin_list(); g; g = g->next) {
		struct plugin *p = (struct plugin *)g->data;
		admin_out(c, "%s", p->ops->name);
	}
}

static void load_module (struct client *c, char **args, void *userdata)
{ 
	if(!args[1]) { 
		admin_out(c, "No file specified");
		return;
	}

   	if(plugin_loaded(args[1])) {
		admin_out(c, "Module already loaded");
		return;
	}

	if (load_plugin(MODULESDIR, args[1])) {
		admin_out(c, "Load successful");
	} else {
		admin_out(c, "Load failed");
	}
}

static void com_save_config (struct client *c, char **args, void *userdata)
{ 
	save_configuration(c->network->global->config, args[1]?args[1]:c->network->global->config->config_dir); 
}

static void help (struct client *c, char **args, void *userdata)
{
	GList *gl = commands;
	char *tmp;
	char **details;
	int i;

	if(args[1]) {
		admin_out(c, "Details for command %s:", args[1]);
	} else {
		admin_out(c, "The following commands are available:");
	}
	while(gl) {
		struct admin_command *cmd = (struct admin_command *)gl->data;
		if(args[1]) {
			if(!g_strcasecmp(args[1], cmd->name)) {
				if(cmd->help_details != NULL) {
					details = g_strsplit(cmd->help_details, "\n", 0);
					for(i = 0; details[i] != NULL; i++) {
						admin_out(c, details[i]);
					}
					return;
				} else {
					admin_out(c, "Sorry, no help for %s available", args[1]);
				}
			}
		} else {
			if(cmd->help != NULL) {
				tmp = g_strdup_printf("%s%s     %s",cmd->name,g_strnfill(longest_command - strlen(cmd->name),' '),cmd->help);
				admin_out(c, tmp);
				g_free(tmp);
			} else {
				admin_out(c, cmd->name);
			}
		}
		gl = gl->next;
	}
	if(args[1]) {
		admin_out(c, "Unknown command");
	}
}

static void list_networks(struct client *c, char **args, void *userdata)
{
	GList *gl;
	for (gl = c->network->global->networks; gl; gl = gl->next) {
		struct network *n = gl->data;

		switch (n->connection.state) {
		case NETWORK_CONNECTION_STATE_NOT_CONNECTED:
			admin_out(c, ("%s: Not connected"), n->name);
			break;
		case NETWORK_CONNECTION_STATE_RECONNECT_PENDING:
			admin_out(c, ("%s: Reconnecting"), n->name);
			break;
		case NETWORK_CONNECTION_STATE_CONNECTED:
		case NETWORK_CONNECTION_STATE_LOGIN_SENT:
		case NETWORK_CONNECTION_STATE_MOTD_RECVD:
			admin_out(c, ("%s: connected"), n->name);
			break;
		}
	}
}

static void detach_client(struct client *c, char **args, void *userdata)
{
	disconnect_client(c, "Client exiting");
}

static void dump_joined_channels(struct client *c, char **args, void *userdata)
{
	struct network *n;
	GList *gl;

	if (args[1] != NULL) {
		n = find_network(c->network->global, args[1]);
		if(n == NULL) {
			admin_out(c, "Can't find network '%s'", args[1]);
			return;
		}
	} else {
		n = c->network;
	}

	if (!n->state) {
		admin_out(c, "Network '%s' not connected", n->name);
		return;
	}

	for (gl = n->state->channels; gl; gl = gl->next) {
		struct channel_state *ch = (struct channel_state *)gl->data;
		admin_out(c, "%s", ch->name);
	}
}

#ifdef DEBUG
static void do_abort(struct client *c, char **args, void *userdata)
{
	abort();
}
#endif

static void handle_die(struct client *c, char **args, void *userdata)
{
	exit(0);
}

static gint cmp_cmd(gconstpointer a, gconstpointer b)
{
	const struct admin_command *cmda = a, *cmdb = b;

	return g_strcasecmp(cmda->name, cmdb->name);
}

void register_admin_command(const struct admin_command *cmd)
{
	commands = g_list_insert_sorted(commands, cmd, cmp_cmd);
	if (strlen(cmd->name) > longest_command) longest_command = strlen(cmd->name);
}

void unregister_admin_command(const struct admin_command *cmd)
{
	commands = g_list_remove(commands, cmd);
}

gboolean admin_process_command(struct client *c, struct line *l, int cmdoffset)
{
	char *tmp, **args = NULL;
	int i;
	GList *gl;

	if(!l->args[cmdoffset]) {
		admin_out(c, "Please specify a command. Use the 'help' command to get a list of available commands");
		return TRUE;
	}

	l->is_private = 1;
	tmp = g_strdup(l->args[cmdoffset]);

	if(l->args[cmdoffset+1]) {
		/* Add everything after l->args[cmdoffset] to tmp */
		for(i = cmdoffset+1; l->args[i]; i++) {
			char *oldtmp = tmp;
			tmp = g_strdup_printf("%s %s", oldtmp, l->args[i]);
			g_free(oldtmp);
		}
	}

	args = g_strsplit(tmp, " ", 0);

	/* Ok, arguments are processed now. Execute the corresponding command */
	gl = commands;
	while(gl) {
		struct admin_command *cmd = (struct admin_command *)gl->data;
		if(!g_strcasecmp(cmd->name, args[0])) {
			cmd->handler(c, args, cmd->userdata);
			g_strfreev(args);
			g_free(tmp);
			return TRUE;
		}
		gl = gl->next;
	}

	admin_out(c, ("Can't find command '%s'. Type 'help' for a list of available commands. "), args[0]);

	g_strfreev(args);
	g_free(tmp);

	return TRUE;
}

static gboolean admin_net_init(struct network *n)
{
	struct channel_state *cs = g_new0(struct channel_state, 1);
	struct channel_nick *user_nick = g_new0(struct channel_nick, 1);
	struct channel_nick *admin_nick = g_new0(struct channel_nick, 1);
	struct network_nick *admin_nnick = g_new0(struct network_nick, 1);
	char *hostmask;
	
	/* Channel */
	cs->name = g_strdup(ADMIN_CHANNEL);
	cs->topic = g_strdup("CtrlProxy administration channel | Type `help' for more information");
	cs->network = n->state;

	/* The users' user */
	user_nick->global_nick = &n->state->me;
	user_nick->channel = cs;
	user_nick->mode = ' ';

	cs->nicks = g_list_append(cs->nicks, user_nick);
	n->state->me.channel_nicks = g_list_append(n->state->me.channel_nicks, user_nick);

	/* CtrlProxy administrator */
	/* global */
	hostmask = g_strdup_printf("ctrlproxy!ctrlproxy@%s", n->name);
	network_nick_set_hostmask(admin_nnick, hostmask);
	g_free(hostmask);

	admin_nnick->fullname = g_strdup("CtrlProxy Admin Tool");
	admin_nnick->channel_nicks = g_list_append(admin_nnick->channel_nicks, admin_nick);

	n->state->nicks = g_list_append(n->state->nicks, admin_nnick);

	/* channel */
	admin_nick->global_nick = admin_nnick;
	admin_nick->channel = cs;
	admin_nick->mode = '@';

	cs->nicks = g_list_append(cs->nicks, admin_nick);
	
	n->state->channels = g_list_append(n->state->channels, cs);

	return TRUE;
}

static gboolean admin_to_server (struct network *n, struct client *c, struct line *l)
{
	if (g_strcasecmp(l->args[0], "PRIVMSG"))
		return TRUE;

	if (g_strcasecmp(l->args[1], ADMIN_CHANNEL)) {
		return TRUE;
	}

	return admin_process_command(c, l, 2);
}

struct virtual_network_ops admin_network = {
	"admin", admin_net_init, admin_to_server, NULL
};

void admin_log(const char *module, enum log_level level, const struct network *n, const struct client *c, const char *data)
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
						  n?n->name:"", 
						  c?"/":"",
						  c?c->description:"",
						  n?")":"");

	for (gl = my_global->networks; gl; gl = gl->next) {
		struct network *network = gl->data;

		if (network->connection.data.virtual.ops != &admin_network)
			continue;

		hostmask = g_strdup_printf("ctrlproxy!ctrlproxy@%s", network->name);
		l = irc_parse_line_args(hostmask, "PRIVMSG", ADMIN_CHANNEL, tmp, NULL); 
		g_free(hostmask);
		
		virtual_network_recv_line(network, l);

		free_line(l);
	}

	g_free(tmp);

	entered = FALSE;
}

const static struct admin_command builtin_commands[] = {
	{ "ADDNETWORK", add_network, "<name>", "Add new network with specified name" },
	{ "ADDSERVER", add_server, "<network> <host>[:<port>] [<password>]", "Add server to network" },
	{ "CONNECT", com_connect_network, "<network>", "Connect to specified network. Forces reconnect when waiting." },
	{ "DELNETWORK", del_network, "<network>", "Remove specified network" },
	{ "NEXTSERVER", com_next_server, "[network]", "Disconnect and use to the next server in the list" },
	{ "DIE", handle_die, "", "Exit ctrlproxy" },
	{ "DISCONNECT", com_disconnect_network, "<network>", "Disconnect specified network" },
	{ "LISTNETWORKS", list_networks, "", "List current networks and their status" },
	{ "LOADMODULE", load_module, "<name>", "Load specified module" },
	{ "LISTMODULES", list_modules, "", "List currently loaded modules" },
	{ "SAVECONFIG", com_save_config, "<name>", "Save current XML configuration to specified file" },
	{ "DETACH", detach_client, "", "Detach current client" },
	{ "HELP", help, "[command]", "This help command" },
	{ "DUMPJOINEDCHANNELS", dump_joined_channels, "[network]", NULL, NULL },
#ifdef DEBUG
	{ "ABORT", do_abort, "", NULL, NULL },
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
}
