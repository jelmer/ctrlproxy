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

	(c) 2003 Jelmer Vernooij <jelmer@nl.linux.org>

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
#include "gettext.h"
#define _(s) gettext(s)

static gboolean without_privmsg = FALSE;
static GList *commands = NULL;
static guint longest_command = 0;

struct admin_command {
	char *name;
	admin_command_handler handler;
	const char *help;
	const char *help_details;
	void *userdata;
	struct plugin *plugin;
};

void admin_out(struct line *l, const char *fmt, ...)
{
	va_list ap;
	char *msg;
	char *hostmask;
	va_start(ap, fmt);
	msg = g_strdup_vprintf(fmt, ap);
	va_end(ap);

	hostmask = g_strdup_printf(":ctrlproxy!ctrlproxy@%s", l->network->name);
	irc_send_args(l->client->incoming, hostmask, "NOTICE", l->network->nick, msg, NULL);

	g_free(hostmask);
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
	if(!args[1]) {
		admin_out(l, _("No name specified"));
		return;
	}

	/* FIXME */
}

static void del_network (char **args, struct line *l, void *userdata)
{
	if(!args[1]) {
		admin_out(l, _("Not enough parameters"));
		return;
	}

	if(find_network_struct(args[1])) {
		admin_out(l, _("Can't remove active network"));
		return;
	}

	/* FIXME */
}

static void add_server (char **args, struct line *l, void *userdata)
{
	if(!args[1] || !args[2]) {
		admin_out(l, _("Not enough parameters"));
		return;
	}

	/* FIXME */
}

static void com_connect_network (char **args, struct line *l, void *userdata)
{
	struct network *s;
	if(!args[1]) {
		 admin_out(l, _("No network specified"));
		 return;
	}

	s = find_network_struct(args[1]);

	if(s && s->reconnect_id == 0) {
		admin_out(l, _("Already connected to %s"), args[1]);
	} else if(s) {
		admin_out(l, _("Forcing reconnect to %s"), args[1]);
		close_server(s);
		connect_current_tcp_server(s);
	} else {
		g_message(_("Connecting to %s"), args[1]);
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
			admin_out(l, _("Can't find active network with that name"));
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
		admin_out(l, _("%s: Not connected"), name);
	} else {
		admin_out(l, _("%s: Reconnecting"), name);
		close_server(n);
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, _("Cycle server in %s"), name);
		connect_next_tcp_server(n);
	}
}

static void list_modules (char **args, struct line *l, void *userdata)
{
	GList *g = get_plugin_list();
	while(g) {
		struct plugin *p = (struct plugin *)g->data;
		admin_out(l, "%s", p->name);
		g = g->next;
	}
}

static void unload_module (char **args, struct line *l, void *userdata)
{
	GList *g = get_plugin_list();

	if(!args[1]) {
		admin_out(l, _("Not enough arguments"));
		return;
	}

	if(!strcmp(args[1], "admin")) {
		admin_out(l, _("Can't unload /this/ module"));
		return;
	}

	/* Find specified plugins' GModule and xmlNodePtr */
	while(g) {
		struct plugin *p = (struct plugin *)g->data;
		if(!strcmp(p->name, args[1])) {
			unload_plugin(p);
			return;
		}
		g = g->next;
	}

	admin_out(l, _("No such plugin loaded"));
}

static void load_module (char **args, struct line *l, void *userdata)
{ 
	if(!args[1]) { 
		admin_out(l, _("No file specified"));
		return;
	}

    if(plugin_loaded(args[1])) {
		admin_out(l, _("Module already loaded"));
		return;
	}

	/* FIXME */
}


static void reload_module (char **args, struct line *l, void *userdata)
{
	unload_module(args, l, NULL);
	load_module(args, l, NULL);
}

static void save_config (char **args, struct line *l, void *userdata)
{ save_configuration(); }

static void help (char **args, struct line *l, void *userdata)
{
	GList *gl = commands;
	char *tmp;
	char **details;
	int i;

	if(args[1]) {
		admin_out(l, _("Details for command %s:"), args[1]);
	} else {
		admin_out(l, _("The following commands are available:"));
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
					admin_out(l, _("Sorry, no help for %s available"), args[1]);
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
		admin_out(l, _("Unknown command"));
	}
}

static void list_networks(char **args, struct line *l, void *userdata)
{
	GList *gl = get_network_list();
	while(gl) {
		struct network *n = gl->data;

		if(!n) admin_out(l, _("%s: Not connected"), n->name);
		else if(n->reconnect_id) admin_out(l, _("%s: Reconnecting"), n->name);
		else admin_out(l, _("%s: connected"), n->name);

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

void register_admin_command(char *name, admin_command_handler handler, const char *help, const char *help_details, void *userdata)
{
	struct admin_command *cmd = g_new(struct admin_command,1);
	cmd->name = g_strdup(name);
	if(longest_command < strlen(name))
		longest_command = strlen(name);
	cmd->handler = handler;
	cmd->help = help;
	cmd->help_details = help_details;
	cmd->plugin = peek_plugin();
	cmd->userdata = userdata;
	commands = g_list_append(commands, cmd);
}

void unregister_admin_command(char *name)
{
	GList *gl = commands;
	while(gl) {
		struct admin_command *cmd = (struct admin_command *)gl->data;
		if(!g_strcasecmp(cmd->name, name)) {
			g_free(cmd->name);
			commands = g_list_remove(commands, cmd);
			g_free(cmd);
			return;
		}
		gl = gl->next;
	}
}

static gboolean handle_data(struct line *l, void *userdata) {
	char *tmp, **args = NULL;
	int cmdoffset = 0;
	int i;
	GList *gl;
	if(l->direction != TO_SERVER) return TRUE;

	if(g_strcasecmp(l->args[0], "CTRLPROXY") == 0)cmdoffset = 1;

	if(!without_privmsg && g_strcasecmp(l->args[0], "PRIVMSG") == 0 &&
	   g_strcasecmp(l->args[1], "CTRLPROXY") == 0) cmdoffset = 2;

	if(cmdoffset == 0) return TRUE;

	if(!l->args[cmdoffset]) {
		admin_out(l, _("Please give a command. Use the 'help' command to get a list of available commands"));
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
			push_plugin(cmd->plugin);
			cmd->handler(args, l, cmd->userdata);
			pop_plugin();
			g_strfreev(args);
			g_free(tmp);
			return TRUE;
		}
		gl = gl->next;
	}

	admin_out(l, _("Can't find command '%s'. Type 'help' for a list of available commands. "), args[0]);

	g_strfreev(args);
	g_free(tmp);

	return TRUE;
}

gboolean fini_plugin(struct plugin *p) {
	del_filter(handle_data);
	return TRUE;
}

const char name_plugin[] = "admin";

gboolean load_config(struct plugin *p, xmlNodePtr node)
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

gboolean init_plugin(struct plugin *p) {
	int i;
	struct admin_command builtin_commands[] = {
		{ "ADDNETWORK", add_network, _("<name>"), _("Add new network with specified name") },
		{ "ADDSERVER", add_server, _("<network> <type> [property1=value1] ..."), _("Add server with specified properties to the specified network") },
		{ "CONNECT", com_connect_network, _("<network>"), _("Connect to specified network. Forces reconnect when waiting.") },
		{ "DELNETWORK", del_network, _("<network>"), _("Remove specified network") },
		{ "NEXTSERVER", com_next_server, _("[network]"), _("Disconnect and use to the next server in the list") },
		{ "DIE", handle_die, "", _("Exit ctrlproxy") },
		{ "DISCONNECT", disconnect_network, _("<network>"), _("Disconnect specified network") },
		{ "LISTNETWORKS", list_networks, "", _("List current networks and their status") },
		{ "LOADMODULE", load_module, "<name>", _("Load specified module") },
		{ "UNLOADMODULE", unload_module, _("<name>"), _("Unload specified module") },
		{ "RELOADMODULE", reload_module, _("<name>"), _("Reload specified module") },
		{ "LISTMODULES", list_modules, "", _("List currently loaded modules") },
		{ "SAVECONFIG", save_config, "<name>", _("Save current XML configuration to specified file") },
		{ "DETACH", detach_client, "", _("Detach current client") },
		{ "HELP", help, "[command]", _("This help command") },
		{ NULL }
	};

	add_filter("admin", handle_data, NULL);

	for(i = 0; builtin_commands[i].name; i++) {
		register_admin_command(builtin_commands[i].name, builtin_commands[i].handler, builtin_commands[i].help, builtin_commands[i].help_details, NULL);
	}

	return TRUE;
}
