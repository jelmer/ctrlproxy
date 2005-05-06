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

#include "ctrlproxy.h"
#include <string.h>

static GHashTable *highlight_backlog = NULL;
static GList *matches = NULL;

static gboolean log_data(struct line *l, void *userdata) {
	struct linestack_context *co = (struct linestack_context *)g_hash_table_lookup(highlight_backlog, l->network);
	GList *gl;

	if(!co) {
		co = linestack_new_by_network(l->network);
		g_hash_table_insert(highlight_backlog, l->network, co);
	}

	if(l->argc < 1)return TRUE;

	if(l->direction == TO_SERVER &&  
	   (!strcasecmp(l->args[0], "PRIVMSG") || !strcasecmp(l->args[0], "NOTICE"))) {
		linestack_clear(co);
		linestack_add_line_list( co, gen_replication_network(l->network));
		return TRUE;
	}

	if(l->direction == TO_SERVER)return TRUE;


	if(strcasecmp(l->args[0], "PRIVMSG") && strcasecmp(l->args[0], "NOTICE")) 
		return TRUE;

	gl = matches;
	while(gl) {
		const char *m = gl->data;
		if(strstr(l->args[1], m) || strstr(l->args[2], m)) {
			linestack_add_line(co, l);
			return TRUE;
		}

		gl = gl->next;
	}

	return TRUE;
}

static gboolean highlight_replicate(struct client *c, void *userdata)
{
	struct linestack_context *replication_data = (struct linestack_context *)g_hash_table_lookup(highlight_backlog, c->network);
	linestack_send(replication_data, c->incoming);
	return TRUE;
}

static gboolean fini_plugin(struct plugin *p) {
	del_replication_filter("repl_highlight");
	del_new_client_hook("repl_highlight");
	g_hash_table_destroy(highlight_backlog); highlight_backlog = NULL;
	return TRUE;
}

static gboolean init_plugin(struct plugin *p) {
	add_replication_filter("repl_highlight", log_data, NULL, 1000);
	add_new_client_hook("repl_highlight", highlight_replicate, NULL);
	highlight_backlog = g_hash_table_new(NULL, NULL);
	return TRUE;
}

struct plugin_ops plugin = {
	.name = "repl_highlight",
	.version = 0,
	.init = init_plugin,
	.fini = fini_plugin
};
