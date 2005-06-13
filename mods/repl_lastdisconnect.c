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

static GHashTable *lastdisconnect_backlog = NULL;

static gboolean log_data(struct line *l, enum data_direction dir, void *userdata) {
	struct linestack_context *co = (struct linestack_context *)g_hash_table_lookup(lastdisconnect_backlog, l->network);
	struct channel_state *c;

	if(!co) {
		co = linestack_new_by_network(l->network);
		g_hash_table_insert( lastdisconnect_backlog, l->network, co);
	}

	if(l->argc < 1)return TRUE;

	if(dir == TO_SERVER)return TRUE;

	if(!g_strcasecmp(l->args[0], "PRIVMSG") ||
	   !g_strcasecmp(l->args[0], "NOTICE") ||
	   !g_strcasecmp(l->args[0], "MODE") || 
	   !g_strcasecmp(l->args[0], "JOIN") || 
	   !g_strcasecmp(l->args[0], "PART") || 
	   !g_strcasecmp(l->args[0], "KICK") || 
	   !g_strcasecmp(l->args[0], "QUIT") ||
	   !g_strcasecmp(l->args[0], "TOPIC") ||
	   !g_strcasecmp(l->args[0], "NICK")) {
		linestack_add_line(co, l);
	} else if(!g_strcasecmp(l->args[0], "353")) {
		c = find_channel(l->network, l->args[3]);
		if(c && !(c->introduced & 2)) {
			linestack_add_line(co, linedup(l));
		}
		/* Only do 366 if not & 2. Set | 2 */
	} else if(!g_strcasecmp(l->args[0], "366")) {
		c = find_channel(l->network, l->args[2]);
		if(c && !(c->introduced & 2)) {
			linestack_add_line(co, linedup(l));
			c->introduced |= 2;
		}
		/* Only do 331 or 332 if not & 1. Set | 1 */
	} else if(!g_strcasecmp(l->args[0], "331") || !g_strcasecmp(l->args[0], "332")) {
		c = find_channel(l->network, l->args[2]);
		if(c && !(c->introduced & 1)) {
			linestack_add_line(co, linedup(l));
			c->introduced |= 1;
		}
	}

	return TRUE;
}

static void lastdisconnect_clear(struct client *c, void *userdata)
{
	struct linestack_context *co = (struct linestack_context *)g_hash_table_lookup(lastdisconnect_backlog, c->network);
	linestack_clear(co);
	linestack_add_line_list( co, gen_replication_network(c->network));
}

static gboolean lastdisconnect_replicate(struct client *c, void *userdata)
{
	struct linestack_context *replication_data = (struct linestack_context *)g_hash_table_lookup(lastdisconnect_backlog, c->network);
	if(!replication_data) {
		replication_data = linestack_new_by_network(c->network);
		linestack_add_line_list(replication_data, gen_replication_network(c->network));
	}
	linestack_send(replication_data, c->incoming);
	return TRUE;
}

static gboolean fini_plugin(struct plugin *p) {
	del_replication_filter("last_disconnect");
	del_lose_client_hook("repl_lastdisconnect");
	del_new_client_hook("repl_lastdisconnect");
	g_hash_table_destroy(lastdisconnect_backlog); lastdisconnect_backlog = NULL;
	return TRUE;
}

static gboolean init_plugin(struct plugin *p) {
	add_replication_filter("repl_lastdisconnect", log_data, NULL, 1000);
	add_lose_client_hook("repl_lastdisconnect", lastdisconnect_clear, NULL);
	add_new_client_hook("repl_lastdisconnect", lastdisconnect_replicate, NULL);
	lastdisconnect_backlog = g_hash_table_new(NULL, NULL);
	return TRUE;
}

struct plugin_ops plugin = {
	.name = "repl_lastdisconnect",
	.version = 0,
	.init = init_plugin,
	.fini = fini_plugin
};
