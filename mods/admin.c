/* 
	ctrlproxy: A modular IRC proxy
	admin: module for remote administration. Available commands:
	 * ADDNETWORK <network>
	 * ADDLISTEN <network> <type> [<key>=<value>] [...]
	 * ADDSERVER <network> <type> [<key>=<value>] [...]
	 * CONNECT <network>
	 * DIE
	 * DISCONNECT [<network>]
	 * LISTNETWORKS
	 * LOADMODULE <location>
	 * RELOADMODULE <location>
	 * UNLOADMODULE <location>
	 * LISTMODULES
	 * DUMPCONFIG 
	 * SAVECONFIG
	 * DETACH
	 * HELP
	
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

#define _GNU_SOURCE
#include "ctrlproxy.h"
#include <string.h>

static gboolean without_privmsg = FALSE;
static GList *commands = NULL;

struct admin_command {
	char *name;
	void (*handler) (char **args, struct line *l);
};

void admin_out(struct line *l, char *fmt, ...)
{
	va_list ap;
	char *msg, *tot, *server_name;
	char *nick;
	va_start(ap, fmt);
	vasprintf(&msg, fmt, ap);
	va_end(ap);

	nick = xmlGetProp(l->network->xmlConf, "nick");
	server_name = xmlGetProp(l->network->xmlConf, "name");

	asprintf(&tot, ":ctrlproxy!ctrlproxy@%s NOTICE %s :%s\r\n", server_name, nick, msg);
	free(msg);

	transport_write(l->client->incoming, tot);
	free(tot);
	xmlFree(nick);
	xmlFree(server_name);
}

static xmlNodePtr find_network_xml(char *name)
{
	char *nname;
	xmlNodePtr cur = xmlNode_networks->xmlChildrenNode;
	while(cur)
	{
		nname = xmlGetProp(cur, "name");
		if(nname && !strcmp(nname, name)){xmlFree(nname); return cur; }
		xmlFree(nname);
		cur = cur->next;
	}
	return NULL;
}

static struct network *find_network_struct(char *name)
{
	char *nname;
	GList *g = networks;
	while(g) {
		struct network *n = (struct network *)g->data;
		nname = xmlGetProp(n->xmlConf, "name");
		if(!strcmp(nname, name)){ xmlFree(nname); return n; }
		xmlFree(nname);
		g = g->next;
	}
	return NULL;
}

static void add_network (char **args, struct line *l)
{ 	
	xmlNodePtr cur;
	if(!args[1]) {
		admin_out(l, "No name specified");
		return;
	}
	
	if(find_network_xml(args[1])) {
		admin_out(l, "Such a network already exists");
		return;
	}

	/* Add node to networks node with specified name */
	cur = xmlNewNode(NULL, "network");
	xmlSetProp(cur, "name", args[1]);
	xmlAddChild(xmlNode_networks, cur);

	/* Add a 'servers' element */
	xmlAddChild(cur, xmlNewNode(NULL, "servers"));
}

static void add_listen (char **args, struct line *l)
{
	xmlNodePtr net, serv, listen;
	struct network *n;
	int i;
	
	if(!args[1] || !args[2]) {
		admin_out(l, "Not enough parameters");
		return;
	}

	net = find_network_xml(args[1]);

	/* Add network if it didn't exist yet */
	if(!net) {
		/* Add node to networks node with specified name */
		net = xmlNewNode(NULL, "network");
		xmlSetProp(net, "name", args[1]);
		xmlAddChild(xmlNode_networks, net);

		/* Add a 'listen' element */
		xmlAddChild(net, xmlNewNode(NULL, "listen"));
	}

	listen = xmlFindChildByElementName(net, "listen");

	/* Add listen node if it didn't exist yet */
	if(!listen) {
		listen = xmlNewNode(NULL, "listen"); 
		xmlAddChild(net, listen); 
	}

	serv = xmlNewNode(NULL, args[2]);
	xmlAddChild(listen, serv);

	for(i = 3; args[i]; i++) {
		char *val = strchr(args[i], '=');
		if(!val) {
			admin_out(l, "Properties should be in the format 'key=value'");
			continue;
		}
		*val = '\0'; val++;
		xmlSetProp(serv, args[i], val);
	}
	
	/* In case the network is active */
	n = find_network_struct(args[1]);
	if(n) {
		if(!n->listen)n->listen = listen;
		network_add_listen(n, serv);
	}
}

static void add_server (char **args, struct line *l)
{
	xmlNodePtr net, serv, servers;
	int i;
	
	if(!args[1] || !args[2]) {
		admin_out(l, "Not enough parameters");
		return;
	}

	net = find_network_xml(args[1]);

	/* Add network if it didn't exist yet */
	if(!net) {
		/* Add node to networks node with specified name */
		net = xmlNewNode(NULL, "network");
		xmlSetProp(net, "name", args[1]);
		xmlAddChild(xmlNode_networks, net);

		/* Add a 'servers' element */
		xmlAddChild(net, xmlNewNode(NULL, "servers"));
	}

	servers = xmlFindChildByElementName(net, "servers");

	/* Add servers node if it didn't exist yet */
	if(!servers) {
		servers = xmlNewNode(NULL, "servers"); 
		xmlAddChild(net, servers); 
	}

	serv = xmlNewNode(NULL, args[2]);
	xmlAddChild(servers, serv);

	for(i = 3; args[i]; i++) {
		char *val = strchr(args[i], '=');
		if(!val) {
			admin_out(l, "Properties should be in the format 'key=value'");
			continue;
		}
		*val = '\0'; val++;
		xmlSetProp(serv, args[i], val);
	}
}

static void com_connect_network (char **args, struct line *l)
{ 
	xmlNodePtr n;
	if(!args[1]) {
		 admin_out(l, "No network specified");
		 return;
	}

	n = find_network_xml(args[1]);
	if(!n) {
		admin_out(l, "Can't find network named %s", args[1]);
		return;
	}

	if(find_network_struct(args[1])) {
		admin_out(l, "Already connected to %s", args[1]);
		return;
	}

	g_message("Connecting to %s", args[1]);
	
	connect_network(n);
}

static void disconnect_network (char **args, struct line *l)
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

static void list_modules (char **args, struct line *l)
{
	GList *g = plugins;
	while(g) {
		struct plugin *p = (struct plugin *)g->data;
		admin_out(l, "%s", p->name);
		g = g->next;
	}
}

static void unload_module (char **args, struct line *l)
{
	GList *g = plugins;

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
		if(!strcmp(p->name, args[1])) { 
			if(unload_plugin(p)) plugins = g_list_remove(plugins, p);
			return; 
		}
		g = g->next;
	}

	admin_out(l, "No such plugin loaded");
}

static void load_module (char **args, struct line *l)
{ 
	xmlNodePtr cur;
	if(!args[1]) { 
		admin_out(l, "No file specified");
		return;
	}

    if(!strcmp(args[1], "admin")) {
		admin_out(l, "Can't load this module /again/");
		return;
	}

	cur = xmlNewNode(NULL, "plugin");
	xmlSetProp(cur, "file", args[1]);
	xmlAddChild(xmlNode_plugins, cur);

	load_plugin(cur);
}


static void reload_module (char **args, struct line *l)
{
	unload_module(args, l);
	load_module(args, l);
}

static void dump_config (char **args, struct line *l)
{ 
	xmlChar *buffer;
	int lastend = 0;
	int size;
	int i;
	char *tmp;
	xmlDocDumpMemory(configuration, &buffer, &size);
	for(i = 0; i < size; i++)
	{
		/* If we encounter a newline or a null-character, we 
		 * print the last line */
		if(buffer[i] != '\n' && buffer[i] != '\0') continue;

		tmp = g_strndup(buffer + lastend, i - lastend);
		admin_out(l, tmp);
		free(tmp);
		lastend = i+1;
	}
}

static void save_config (char **args, struct line *l)
{ save_configuration(); }

static void help (char **args, struct line *l)
{
	GList *gl = commands;

	if(args[1]) {
		/* FIXME: Read /usr/share/ctrlproxy/help/commands/args[1] and send it to the user */
	}
		
	admin_out(l, "The following commands are available:");
	while(gl) {
		struct admin_command *cmd = (struct admin_command *)gl->data;
		admin_out(l, cmd->name);
		gl = gl->next;
	}
}

static void list_networks(char **args, struct line *l)
{
	char *nname;
	xmlNodePtr p = xmlNode_networks;
	while(p) {
		struct network *n;
		nname = xmlGetProp(p, "name");
		n = find_network_struct(nname);
		if(!n) admin_out(l, "%s: Not connected", nname);
		else if(n->reconnect_id) admin_out(l, "%s: Reconnecting", nname);
		else admin_out(l, "%s: connected", nname);
		xmlFree(nname);
		p = p->next;
	}
}

static void detach_client(char **args, struct line *l)
{
	disconnect_client(l->client);
	l->client = NULL;
}

static void handle_die(char **args, struct line *l) 
{
	g_main_loop_quit(main_loop);
}

void register_admin_command(char *name, void (*handler) (char **args, struct line *l))
{
	struct admin_command *cmd = malloc(sizeof(struct admin_command));
	cmd->name = strdup(name);
	cmd->handler = handler;
	commands = g_list_append(commands, cmd);
}

void unregister_admin_command(char *name)
{
	GList *gl = commands;
	while(gl) {
		struct admin_command *cmd = (struct admin_command *)gl->data;
		if(!strcasecmp(cmd->name, name)) {
			free(cmd->name);
			commands = g_list_remove(commands, cmd);
			free(cmd);
			return;
		}
		gl = gl->next;
	}
}

static struct admin_command builtin_commands[] = {
	{ "ADDNETWORK", add_network },
	{ "ADDSERVER", add_server },
	{ "ADDLISTEN", add_listen },
	{ "CONNECT", com_connect_network },
	{ "DIE", handle_die },
	{ "DISCONNECT", disconnect_network },
	{ "LISTNETWORKS", list_networks },
	{ "LOADMODULE", load_module },
	{ "UNLOADMODULE", unload_module },
	{ "RELOADMODULE", reload_module },
	{ "LISTMODULES", list_modules },
	{ "DUMPCONFIG", dump_config },
	{ "SAVECONFIG", save_config },
	{ "DETACH", detach_client },
	{ "HELP", help },
	{ NULL }
};

static gboolean handle_data(struct line *l) {
	char *tmp, **args = NULL;
	int cmdoffset = 0;
	GError *error = NULL;
	int argc = 0;
	int i;
	GList *gl;
	if(l->direction != TO_SERVER) return TRUE;

	if(strcasecmp(l->args[0], "CTRLPROXY") == 0)cmdoffset = 1;

	if(!without_privmsg && strcasecmp(l->args[0], "PRIVMSG") == 0 && 
	   strcasecmp(l->args[1], "CTRLPROXY") == 0) cmdoffset = 2;

	if(cmdoffset == 0) return TRUE;

	if(!l->args[cmdoffset]) {
		admin_out(l, "Please give a command. Use the 'help' command to get a list of available commands");
		return TRUE;
	}

	args = malloc(sizeof(char *) * 2);
	l->options |= LINE_DONT_SEND | LINE_IS_PRIVATE;
	tmp = strdup(l->args[cmdoffset]);

	if(l->args[cmdoffset+1]) {
		/* Add everything after l->args[cmdoffset] to tmp */
		for(i = cmdoffset+1; l->args[i]; i++) {
			char *oldtmp = tmp;
			asprintf(&tmp, "%s %s", oldtmp, l->args[i]);
			free(oldtmp);
		}
	}

	if(!g_shell_parse_argv(tmp, &argc, &args, &error)) {
		admin_out(l, "Parse error of argument %s", error->message);
		return FALSE;
	}

	/* Ok, arguments are processed now. Execute the corresponding command */
	gl = commands;
	while(gl) {
		struct admin_command *cmd = (struct admin_command *)gl->data;
		if(!strcasecmp(cmd->name, args[0])) {
			cmd->handler(args, l);
			g_strfreev(args);
			free(tmp);
			return TRUE;
		}
		gl = gl->next;
	}

	admin_out(l, "Can't find command '%s'. Type 'help' for a list of available commands. ", args[0]);

	g_strfreev(args);
	free(tmp);

	return TRUE;
}

gboolean fini_plugin(struct plugin *p) {
	del_filter(handle_data);
	return TRUE;
}

gboolean init_plugin(struct plugin *p) {
	xmlNodePtr cur;
	int i;

	add_filter("admin", handle_data);
	cur = xmlFindChildByElementName(p->xmlConf, "without_privmsg");
	if(cur) without_privmsg = TRUE;

	for(i = 0; builtin_commands[i].name; i++) {
		register_admin_command(builtin_commands[i].name, builtin_commands[i].handler);
	}
	
	return TRUE;
}
