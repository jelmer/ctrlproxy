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

static GHashTable *files = NULL;

void replicate_data(struct network *s, struct transport_context *io) {
	GSList *l = g_hash_table_lookup(files, s);
	if(!l) { default_replicate_function(s, io); return; }
	while(l) {
		struct line *rl = (struct line *)l->data;
		if(rl) {
			irc_send_line(io, rl);
			free_line(rl);
		}
		l = g_slist_next(l);
	}
	g_slist_free(l);
	g_hash_table_replace(files, s, NULL);
}

static gboolean log_data(struct line *l) {
	GSList *gl = g_hash_table_lookup(files, l->network);
	struct channel *c;

	if(l->argc < 1)return TRUE;

	if(l->direction == TO_SERVER &&  
	   (!strcasecmp(l->args[0], "PRIVMSG") || !strcasecmp(l->args[0], "NOTICE"))) {
		GSList *gs = gl;
		while(gs) {
			if(gs->data)free_line((struct line *)gs->data);
			gs = g_slist_next(gs);
		}
		
		g_slist_free(gl);
		g_hash_table_replace(files, l->network, NULL);
		return TRUE;
	}

	if(l->direction == TO_SERVER)return TRUE;

	if(!gl) gl = gen_replication(l->network);

	if(!strcasecmp(l->args[0], "PRIVMSG") ||
	   !strcasecmp(l->args[0], "NOTICE") ||
	   !strcasecmp(l->args[0], "MODE") || 
	   !strcasecmp(l->args[0], "JOIN") || 
	   !strcasecmp(l->args[0], "PART") || 
	   !strcasecmp(l->args[0], "KICK") || 
	   !strcasecmp(l->args[0], "QUIT") ||
	   !strcasecmp(l->args[0], "TOPIC") ||
	   !strcasecmp(l->args[0], "NICK")) {
		gl = g_slist_append(gl, linedup(l));
	} else if(!strcasecmp(l->args[0], "353")) {
		c = find_channel(l->network, l->args[3]);
		if(c && !(c->introduced & 2)) {
			gl = g_slist_append(gl, linedup(l));
		}
		/* Only do 366 if not & 2. Set | 2 */
	} else if(!strcasecmp(l->args[0], "366")) {
		c = find_channel(l->network, l->args[2]);
		if(c && !(c->introduced & 2)) {
			gl = g_slist_append(gl, linedup(l));
			c->introduced |= 2;
		}
		/* Only do 331 or 332 if not & 1. Set | 1 */
	} else if(!strcasecmp(l->args[0], "331") || !strcasecmp(l->args[0], "332")) {
		c = find_channel(l->network, l->args[2]);
		if(c && !(c->introduced & 1)) {
			gl = g_slist_append(gl, linedup(l));
			c->introduced |= 1;
		}
	}

	g_hash_table_replace(files, l->network, gl);
	return TRUE;
}

gboolean fini_plugin(struct plugin *p) {
	del_filter(log_data);
	replicate_function = default_replicate_function;
	return TRUE;
}

gboolean init_plugin(struct plugin *p) {
	files = g_hash_table_new(NULL, NULL);
	replicate_function = replicate_data;
	add_filter("repl_memory", log_data);
	return TRUE;
}
