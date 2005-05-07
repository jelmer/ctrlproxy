/*
	ctrlproxy: A modular IRC proxy
	admin: module for remote administration. Available commands:
	 * ADDNETWORK <network>
	 * ADDLISTEN <network> <type> [<key>=<value>] [...]
	 * ADDSERVER <network> <type> [<key>=<value>] [...]
	 * CONNECT <network>
	 * DELNETWORK <network>
	 * DETACH
	 * DIE
	 * DISCONNECT [<network>]
	 * DUMPCONFIG
	 * HELP
	 * LISTMODULES
	 * LISTNETWORKS
	 * LOADMODULE <location>
	 * NEXTSERVER <network>
	 * RELOADMODULE <location>
	 * SAVECONFIG
	 * UNLOADMODULE <location>

	(c) 2003-2005 Jelmer Vernooij <jelmer@nl.linux.org>

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

#define ADMIN_CORE_BUILD
#include "ctrlproxy.h"
#include <string.h>
#include "admin.h"

#define ADMIN_CHANNEL "#ctrlproxy"

static gboolean without_privmsg = FALSE;
static GList *commands = NULL;
static guint longest_command = 0;

void admin_out(struct line *l, const char *fmt, ...)
{
	va_list ap;
	char *msg;
	char *hostmask;
	va_start(ap, fmt);
	msg = g_strdup_vprintf(fmt, ap);
	va_end(ap);

	hostmask = g_strdup_printf(":ctrlproxy!ctrlproxy@%s", l->network->name);
	if (l->network->type == NETWORK_VIRTUAL && 
		!strcmp(l->network->connection.virtual.ops->name, "admin") &&
		!strcmp(l->args[1], ADMIN_CHANNEL)) {
		virtual_network_recv_args(l->network, hostmask+1, "PRIVMSG", ADMIN_CHANNEL, msg, NULL);
	} else {
		irc_send_args(l->client->incoming, hostmask, "NOTICE", l->network->nick, msg, NULL);
	}
	g_free(hostmask);

	g_free(msg);
}

static struct network *find_network_struct(char *name)
{
	GList *g = get_network_list();
	while(g) {
		struct network *n = (struct network *)g->data;
		if(n->name && !strcmp(n->name, name)) 
			return n;
		g = g->next;
	}
	return NULL;
}

static void add_network (char **args, struct line *l, void *userdata)
{
	struct network *n;
	if(!args[1]) {
		admin_out(l, "No name specified");
		return;
	}

	n = new_network();
	g_free(n->name); n->name = g_strdup(args[1]);
}

static void del_network (char **args, struct line *l, void *userdata)
{
	struct network *n;

	if(!args[1]) {
		admin_out(l, "Not enough parameters");
		return;
	}

	n = find_network_struct(args[1]);
	if (!n) {
		admin_out(l, "No such network %s", args[1]);
		return;
	}

	close_network(n);
}

static void add_server (char **args, struct line *l, void *userdata)
{
	struct network *n;
	struct tcp_server *s;

	if(!args[1] || !args[2]) {
		admin_out(l, "Not enough parameters");
		return;
	}

	n = find_network_struct(args[1]);

	if (!n) {
		admin_out(l, "No such network '%s'", args[1]);
		return;
	}

	if (n->type != NETWORK_TCP) {
		admin_out(l, "Not a TCP/IP network!");
		return;
	}

	s = g_new0(struct tcp_server, 1);

	s->name = g_strdup(args[2]);
	s->host = g_strdup(args[2]);
	s->port = g_strdup(args[3]?args[3]:"6667");
	s->ssl = FALSE;
	s->password = (args[3] && args[4])?g_strdup(args[4]):NULL;

	n->connection.tcp.servers = g_list_append(n->connection.tcp.servers, s);
}

static void com_connect_network (char **args, struct line *l, void *userdata)
{
	struct network *s;
	if(!args[1]) {
		 admin_out(l, "No network specified");
		 return;
	}

	s = find_network_struct(args[1]);

	if(s && s->reconnect_id == 0) {
		admin_out(l, "Already connected to %s", args[1]);
	} else if(s) {
		admin_out(l, "Forcing reconnect to %s", args[1]);
		close_server(s);
		connect_current_tcp_server(s);
	} else {
		admin_out(l, "Connecting to %s", args[1]);
		connect_network(s);
	}
}

static void disconnect_network (char **args, struct line *l, void *userdata)
{
	struct network *n;
	if(!args[1])n = l->network;
	else {
		n = find_network_struct(args[1]);
		if(!n) {
			admin_out(l, "Can't find active network with that name");
			return;
		}
	}

	close_network(n);
}

static void com_next_server (char **args, struct line *l, void *userdata) {
	struct network *n;
	const char *name;
	if(args[1] != NULL) {
		name = args[1];
		n = find_network_struct(args[1]);
	} else {
		n = l->network;
		name = n->name;
	}
	if(!n) {
		admin_out(l, "%s: Not connected", name);
	} else {
		admin_out(l, "%s: Reconnecting", name);
		close_server(n);
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "Cycle server in %s", name);
		connect_next_tcp_server(n);
	}
}

static void list_modules (char **args, struct line *l, void *userdata)
{
	GList *g = get_plugin_list();
	while(g) {
		struct plugin *p = (struct plugin *)g->data;
		admin_out(l, "%s", p->ops->name);
		g = g->next;
	}
}

static void unload_module (char **args, struct line *l, void *userdata)
{
	GList *g = get_plugin_list();

	if(!args[1]) {
		admin_out(l, "Not enough arguments");
		return;
	}

	if(!strcmp(args[1], "admin")) {
		admin_out(l, "Can't unload /this/ module");
		return;
	}

	/* Find specified plugins' GModule and xmlNodePtr */
	while(g) {
		struct plugin *p = (struct plugin *)g->data;
		if(!strcmp(p->ops->name, args[1])) {
			unload_plugin(p);
			return;
		}
		g = g->next;
	}

	admin_out(l, "No such plugin loaded");
}

static void load_module (char **args, struct line *l, void *userdata)
{ 
	struct plugin *p;
	
	if(!args[1]) { 
		admin_out(l, "No file specified");
		return;
	}

    	if(plugin_loaded(args[1])) {
		admin_out(l, "Module already loaded");
		return;
	}

	p = new_plugin(args[1]);

	if (load_plugin(p)) {
		admin_out(l, "Load successful");
	} else {
		admin_out(l, "Load failed");
	}
}

static void reload_module (char **args, struct line *l, void *userdata)
{
	unload_module(args, l, NULL);
	load_module(args, l, NULL);
}

static void com_save_config (char **args, struct line *l, void *userdata)
{ save_configuration(args[1]); }

static void help (char **args, struct line *l, void *userdata)
{
	GList *gl = commands;
	char *tmp;
	char **details;
	int i;

	if(args[1]) {
		admin_out(l, "Details for command %s:", args[1]);
	} else {
		admin_out(l, ("The following commands are available:"));
	}
	while(gl) {
		struct admin_command *cmd = (struct admin_command *)gl->data;
		if(args[1]) {
			if(!g_strcasecmp(args[1], cmd->name)) {
				if(cmd->help_details != NULL) {
					details = g_strsplit(cmd->help_details, "\n", 0);
					for(i = 0; details[i] != NULL; i++) {
						admin_out(l, details[i]);
					}
					return;
				} else {
					admin_out(l, ("Sorry, no help for %s available"), args[1]);
				}
			}
		} else {
			if(cmd->help != NULL) {
				tmp = g_strdup_printf("%s%s     %s",cmd->name,g_strnfill(longest_command - strlen(cmd->name),' '),cmd->help);
				admin_out(l, tmp);
				g_free(tmp);
			} else {
				admin_out(l, cmd->name);
			}
		}
		gl = gl->next;
	}
	if(args[1]) {
		admin_out(l, ("Unknown command"));
	}
}

static void list_networks(char **args, struct line *l, void *userdata)
{
	GList *gl = get_network_list();
	while(gl) {
		struct network *n = gl->data;

		if(!n) admin_out(l, ("%s: Not connected"), n->name);
		else if(n->reconnect_id) admin_out(l, ("%s: Reconnecting"), n->name);
		else admin_out(l, ("%s: connected"), n->name);

		gl = gl->next;
	}
}

static void detach_client(char **args, struct line *l, void *userdata)
{
	disconnect_client(l->client);
	l->client = NULL;
}

static void handle_die(char **args, struct line *l, void *userdata)
{
	exit(0);
}

void register_admin_command(const struct admin_command *cmd)
{
	commands = g_list_append(commands, cmd);
	if (strlen(cmd->name) > longest_command) longest_command = strlen(cmd->name);
}

void unregister_admin_command(const struct admin_command *cmd)
{
	commands = g_list_remove(commands, cmd);
}

static gboolean process_command(struct line *l, int cmdoffset)
{
	char *tmp, **args = NULL;
	int i;
	GList *gl;

	if(!l->args[cmdoffset]) {
		admin_out(l, ("Please give a command. Use the 'help' command to get a list of available commands"));
		return TRUE;
	}

	l->options |= LINE_DONT_SEND | LINE_IS_PRIVATE;
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
			cmd->handler(args, l, cmd->userdata);
			g_strfreev(args);
			g_free(tmp);
			return TRUE;
		}
		gl = gl->next;
	}

	admin_out(l, ("Can't find command '%s'. Type 'help' for a list of available commands. "), args[0]);

	g_strfreev(args);
	g_free(tmp);

	return TRUE;
}

static gboolean handle_data(struct line *l, void *userdata) {
	int cmdoffset = 0;

	if(l->direction != TO_SERVER) 
		return TRUE;

	if(g_strcasecmp(l->args[0], "CTRLPROXY") == 0)cmdoffset = 1;

	if(!without_privmsg && g_strcasecmp(l->args[0], "PRIVMSG") == 0 &&
	   g_strcasecmp(l->args[1], "CTRLPROXY") == 0) cmdoffset = 2;

	if(cmdoffset == 0) 
		return TRUE;

	process_command(l, cmdoffset);

	return TRUE;
}

static gboolean save_config(struct plugin *p, xmlNodePtr node)
{
	if (without_privmsg) {
		xmlAddChild(node,xmlNewNode(NULL, "without_privmsg"));
	}

	return TRUE;
}

static gboolean load_config(struct plugin *p, xmlNodePtr node)
{
	xmlNodePtr cur;

	for (cur = node->children; cur; cur = cur->next)
	{
		if (cur->type != XML_ELEMENT_NODE) continue;	

		if (!strcmp(cur->name, "without_privmsg")) {
			without_privmsg = TRUE;
		}
	}

	return TRUE;
}

static gboolean admin_net_init(struct network *n)
{
	n->authenticated = TRUE;
	virtual_network_recv_args(n, n->hostmask, "JOIN", ADMIN_CHANNEL, NULL);
	virtual_network_recv_args(n, n->name, "332", n->nick, ADMIN_CHANNEL, "CtrlProxy administration channel", NULL);
	virtual_network_recv_args(n, n->name, "353", n->nick, "=", ADMIN_CHANNEL, n->nick, NULL);
	virtual_network_recv_args(n, n->name, "366", n->nick, ADMIN_CHANNEL, "End of /names list", NULL);

	return TRUE;
}

static gboolean admin_to_server (struct network *n, struct line *l)
{
	if (strcmp(l->args[0], "PRIVMSG"))
		return TRUE;

	if (strcmp(l->args[1], ADMIN_CHANNEL)) {
		return TRUE;
	}

	return process_command(l, 2);
}

struct virtual_network_ops admin_network = {
	"admin",
	admin_net_init, 
	admin_to_server,
	NULL
};

static gboolean fini_plugin(struct plugin *p) {
	unregister_virtual_network(&admin_network);
	del_client_filter("admin");
	return TRUE;
}

static gboolean init_plugin(struct plugin *p) {
	int i;
	const static struct admin_command builtin_commands[] = {
		{ "ADDNETWORK", add_network, ("<name>"), ("Add new network with specified name") },
		{ "ADDSERVER", add_server, ("<network> <host> [<port> [<password>]]"), ("Add server to network") },
		{ "CONNECT", com_connect_network, ("<network>"), ("Connect to specified network. Forces reconnect when waiting.") },
		{ "DELNETWORK", del_network, ("<network>"), ("Remove specified network") },
		{ "NEXTSERVER", com_next_server, ("[network]"), ("Disconnect and use to the next server in the list") },
		{ "DIE", handle_die, "", ("Exit ctrlproxy") },
		{ "DISCONNECT", disconnect_network, ("<network>"), ("Disconnect specified network") },
		{ "LISTNETWORKS", list_networks, "", ("List current networks and their status") },
		{ "LOADMODULE", load_module, "<name>", ("Load specified module") },
		{ "UNLOADMODULE", unload_module, ("<name>"), ("Unload specified module") },
		{ "RELOADMODULE", reload_module, ("<name>"), ("Reload specified module") },
		{ "LISTMODULES", list_modules, "", ("List currently loaded modules") },
		{ "SAVECONFIG", com_save_config, "<name>", ("Save current XML configuration to specified file") },
		{ "DETACH", detach_client, "", ("Detach current client") },
		{ "HELP", help, "[command]", ("This help command") },
		{ NULL }
	};

	add_client_filter("admin", handle_data, NULL, 1);

	for(i = 0; builtin_commands[i].name; i++) {
		register_admin_command(&builtin_commands[i]);
	}

	register_virtual_network(&admin_network);

	return TRUE;
}

struct plugin_ops plugin = {
	.name = "admin",
	.version = 0,
	.init = init_plugin,
	.fini = fini_plugin,
	.load_config = load_config,
	.save_config = save_config
};
