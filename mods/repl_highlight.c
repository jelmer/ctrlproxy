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

static xmlNodePtr xmlConf;
static GHashTable *highlight_backlog = NULL;

static gboolean log_data(struct line *l) {
	struct linestack_context *co = (struct linestack_context *)g_hash_table_lookup(highlight_backlog, l->network);
	xmlNodePtr cur;
	struct channel *c;

	if(!co) {
		co = linestack_new_by_network(l->network);
		linestack_add_line_list( co, gen_replication_network(l->network));
		g_hash_table_insert(highlight_backlog, l->network, co);
	}

	if(l->argc < 1)return TRUE;

	if(l->direction == TO_SERVER &&  
	   (!strcasecmp(l->args[0], "PRIVMSG") || !strcasecmp(l->args[0], "NOTICE"))) {
		linestack_clear(co);
		g_hash_table_remove(highlight_backlog, l->network);
		return TRUE;
	}

	if(l->direction == TO_SERVER)return TRUE;


	if(strcasecmp(l->args[0], "PRIVMSG") && strcasecmp(l->args[0], "NOTICE")) 
		return TRUE;

	cur = xmlConf->xmlChildrenNode;
	while(cur) {

		if(!xmlIsBlankNode(cur) && !strcmp(cur->name, "match")) {
			if(strstr(l->args[1], xmlNodeGetContent(cur)) || strstr(l->args[2], xmlNodeGetContent(cur))) {
				linestack_add_line(co, l);
				return TRUE;
			}
		}

		cur = cur->next;
	}

	return TRUE;
}

static gboolean highlight_replicate(struct client *c)
{
	struct linestack_context *replication_data = (struct linestack_context *)g_hash_table_lookup(highlight_backlog, c->network);
	if(!replication_data) {
		replication_data = linestack_new_by_network(c->network); 
		linestack_add_line_list(replication_data, gen_replication_network(c->network));
	}
	linestack_send(replication_data, c->incoming);
	return TRUE;
}

gboolean fini_plugin(struct plugin *p) {
	del_filter(log_data);
	del_new_client_hook("repl_highlight");
	g_hash_table_destroy(highlight_backlog); highlight_backlog = NULL;
	return TRUE;
}

gboolean init_plugin(struct plugin *p) {
	xmlConf = p->xmlConf;
	add_filter("repl_highlight", log_data);
	add_new_client_hook("repl_highlight", highlight_replicate);
	highlight_backlog = g_hash_table_new(NULL, NULL);
	return TRUE;
}

