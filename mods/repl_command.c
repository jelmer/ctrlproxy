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

static gboolean log_data(struct line *l) {
	GHashTable *h = (GHashTable *)l->network->replication_data;
	struct linestack_context *co;
	struct channel *c;


	if(l->argc < 1)return TRUE;

	if(strcasecmp(l->args[0], "PRIVMSG") && strcasecmp(l->args[0], "NOTICE"))return TRUE;

	/* Add hash table if hasn't been added yet */
	if(!h) {
		h = g_hash_table_new(g_str_hash, g_str_equal);
		l->network->replication_data = h;
	}

	/* Lookup this channel */
	co = g_hash_table_lookup(h, l->args[1]);
	if(!co) co = linestack_new_by_network(l->network);

	linestack_add_line_list( co, gen_replication(l->network));

	return TRUE;
}

gboolean fini_plugin(struct plugin *p) {
	del_filter(log_data);
	unregister_admin_command("BACKLOG");
	return TRUE;
}

static void replicate_channel(gpointer key, gpointer val, gpointer user)
{
	struct linestack_context *co = (struct linestack_context *)val;
	struct client *c = (struct client *)user;

	linestack_send(co, c->incoming);
	linestack_clear(co);
}

static void repl_command(char **args, struct line *l)
{
	GHashTable *h = (GHashTable *)l->network->replication_data;
	struct linestack_context *co;

	if(!h) return;
	
	if(!args[1]) {
		/* Backlog everything for this network */
		g_hash_table_foreach(h, replicate_channel, l->client);
		return;
	} 

	/* Backlog for specific nick/channel */
	co = g_hash_table_lookup(h, args[1]);

	if(co)  {
		linestack_send(co, l->client->incoming);
		linestack_clear(co);
	}
}

gboolean init_plugin(struct plugin *p) {
	char *ival;
	if(!plugin_loaded("admin")) {
		g_warning("admin module required for repl_command module. Please load it first");
		return FALSE;
	}
	add_filter("repl_command", log_data);
	register_admin_command("BACKLOG", repl_command);
	return TRUE;
}
