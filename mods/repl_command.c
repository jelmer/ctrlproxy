/* 
	ctrlproxy: A modular IRC proxy
	(c) 2002-2003 Jelmer Vernooij <jelmer@nl.linux.org>

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

static GHashTable *command_backlog = NULL;

static gboolean log_data(struct line *l) {
	struct linestack_context *co;
	struct channel *c;
	char *desc, *networkname;

	if(l->argc < 1)return TRUE;

	if(strcasecmp(l->args[0], "PRIVMSG") && strcasecmp(l->args[0], "NOTICE"))return TRUE;

	/* Lookup this channel */
	networkname = xmlGetProp(l->network->xmlConf, "name");
	asprintf(&desc, "%s/%s", networkname, l->args[1]);
	xmlFree(networkname);
	
	co = g_hash_table_lookup(command_backlog, desc);
	
	if(!co) {
		co = linestack_new_by_network(l->network);
		g_hash_table_insert(command_backlog, desc, co);
		linestack_add_line_list( co, gen_replication(l->network));
	}
	linestack_add_line(co, l);

	free(desc);

	return TRUE;
}

gboolean fini_plugin(struct plugin *p) {
	del_filter(log_data);
	unregister_admin_command("BACKLOG");
	g_hash_table_destroy(command_backlog); command_backlog = NULL;
	return TRUE;
}

static void replicate_channel(gpointer key, gpointer val, gpointer user)
{
	struct linestack_context *co = (struct linestack_context *)val;
	struct line *l = (struct line *)user;
	char *networkname = xmlGetProp(l->network->xmlConf, "name");

	if(strncasecmp(networkname, key, strlen(networkname))) {
		xmlFree(networkname);
		return;
	}
	xmlFree(networkname);

	linestack_send(co, l->client->incoming);
	linestack_clear(co);
	g_hash_table_remove(command_backlog, key);
}

static void repl_command(char **args, struct line *l)
{
	struct linestack_context *co;
	char *networkname, *desc;

	if(!command_backlog) { 
		admin_out(l, "No backlogs saved yet");
		return;
	}
	
	
	if(!args[1]) {
		char *networkname = xmlGetProp(l->network->xmlConf, "name");
		admin_out(l, "Sending backlog for network '%s'", networkname);
		xmlFree(networkname);

		/* Backlog everything for this network */
		g_hash_table_foreach(command_backlog, replicate_channel, l);
		return;
	} 

	/* Backlog for specific nick/channel */
	networkname = xmlGetProp(l->network->xmlConf, "name");
	admin_out(l, "Sending backlog for channel %s@%s", args[1], networkname);
	asprintf(&desc, "%s/%s", networkname, args[1]);
	xmlFree(networkname);
	co = g_hash_table_lookup(command_backlog, desc);
	free(desc);

	if(co)  {
		linestack_send(co, l->client->incoming);
		linestack_clear(co);
		g_hash_table_remove(command_backlog, desc);
	} else {
		admin_out(l, "No backlog for %s", args[1]);
	}
}

gboolean init_plugin(struct plugin *p) {
	char *ival;
	if(!plugin_loaded("admin") && !plugin_loaded("libadmin")) {
		g_warning("admin module required for repl_command module. Please load it first");
		return FALSE;
	}
	add_filter("repl_command", log_data);
	register_admin_command("BACKLOG", repl_command);
	command_backlog = g_hash_table_new(g_str_hash, g_str_equal);
	return TRUE;
}
