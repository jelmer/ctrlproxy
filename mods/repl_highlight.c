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

xmlNodePtr xmlConf;

void replicate_data(struct network *s, struct transport_context *io) {
	if(!s->replication_data) { default_replicate_function(s, io); return; }
	linestack_send(s->replication_data, io);
}

static gboolean log_data(struct line *l) {
	struct linestack_context *co = (struct linestack_context *)l->network->replication_data;
	xmlNodePtr cur;
	struct channel *c;

	if(!co) co = linestack_new_by_network(l->network);

	if(l->argc < 1)return TRUE;

	if(l->direction == TO_SERVER &&  
	   (!strcasecmp(l->args[0], "PRIVMSG") || !strcasecmp(l->args[0], "NOTICE"))) {
		linestack_clear(co);
		return TRUE;
	}

	if(l->direction == TO_SERVER)return TRUE;

	if(!co) linestack_add_line_list( co, gen_replication(l->network));

	if(strcasecmp(l->args[0], "PRIVMSG") && strcasecmp(l->args[0], "NOTICE")) 
		return TRUE;

	cur = xmlConf->xmlChildrenNode;
	while(cur) {

		if(!xmlIsBlankNode(cur) && !strcmp(cur->name, "match")) {
			if(strstr(l->args[1], xmlNodeGetContent(cur)) || strstr(l->args[2], xmlNodeGetContent(cur))) {
				linestack_add_line(co, linedup(l));
				return TRUE;
			}
		}

		cur = cur->next;
	}

	return TRUE;
}

gboolean fini_plugin(struct plugin *p) {
	del_filter(log_data);
	replicate_function = default_replicate_function;
	return TRUE;
}

gboolean init_plugin(struct plugin *p) {
	xmlConf = p->xmlConf;
	replicate_function = replicate_data;
	add_filter("repl_highlight", log_data);
	return TRUE;
}
