/* 
	ctrlproxy: A modular IRC proxy
	(c) 2004 Jelmer Vernooij <jelmer@nl.linux.org>

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

static GHashTable *limited_backlog = NULL;

static gboolean log_data(struct line *l) {
	struct linestack_context *co = (struct linestack_context *)g_hash_table_lookup(limited_backlog, l->network);

	if(!co) {
		co = linestack_new_by_network(l->network);
		g_hash_table_insert(limited_backlog, l->network, co);
	}

	if(l->argc < 1)return TRUE;

	if(l->direction == TO_SERVER &&  
	   (!g_ascii_strcasecmp(l->args[0], "PRIVMSG") || !g_ascii_strcasecmp(l->args[0], "NOTICE"))) {
		linestack_clear(co);
		linestack_add_line_list( co, gen_replication_network(l->network));
		return TRUE;
	}

	if(l->direction == FROM_SERVER && 
	   (!g_ascii_strcasecmp(l->args[0], "PRIVMSG") || !g_ascii_strcasecmp(l->args[0], "NOTICE"))) 
		linestack_add_line(co, l);

	return TRUE;
}

static gboolean limited_replicate(struct client *c)
{
	struct linestack_context *replication_data = (struct linestack_context *)g_hash_table_lookup(limited_backlog, c->network);
	linestack_send(replication_data, c->incoming);
	return TRUE;
}

gboolean fini_plugin(struct plugin *p) {
	del_filter(log_data);
	del_new_client_hook("repl_limited");
	g_hash_table_destroy(limited_backlog); limited_backlog = NULL;
	return TRUE;
}

const char name_plugin[] = "repl_limited";

gboolean init_plugin(struct plugin *p) {
	add_filter_ex("repl_limited", log_data, "replicate", 1000);
	add_new_client_hook("repl_limited", limited_replicate);
	limited_backlog = g_hash_table_new(NULL, NULL);
	return TRUE;
}

