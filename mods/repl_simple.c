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

static GHashTable *simple_backlog = NULL;
static GHashTable *simple_initialnick = NULL;

static void change_nick(struct client *c, char *newnick) 
{
	struct line *l = irc_parse_line_args(c->network->hostmask, "NICK", newnick, NULL);
	irc_send_line(c->incoming, l);
	free_line(l);
}

static gboolean log_data(struct line *l, void *userdata) {
	struct linestack_context *co = (struct linestack_context *)g_hash_table_lookup(simple_backlog, l->network);
	struct channel *c;

	if(!co) {
		co = linestack_new_by_network(l->network);
		g_hash_table_insert( simple_backlog, l->network, co);
		g_hash_table_insert( simple_initialnick, l->network, l->network->nick);
	}

	if(l->argc < 1)return TRUE;

	if(l->direction == TO_SERVER &&  
	   (!g_strcasecmp(l->args[0], "PRIVMSG") || !g_strcasecmp(l->args[0], "NOTICE"))) {
		linestack_clear(co);
		g_hash_table_replace( simple_initialnick, l->network, l->network->nick);
		linestack_add_line_list( co, gen_replication_network(l->network));
		return TRUE;
	}

	if(l->direction == TO_SERVER)return TRUE;


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
			linestack_add_line(co, l);
		}
		/* Only do 366 if not & 2. Set | 2 */
	} else if(!g_strcasecmp(l->args[0], "366")) {
		c = find_channel(l->network, l->args[2]);
		if(c && !(c->introduced & 2)) {
			linestack_add_line(co, l);
			c->introduced |= 2;
		}
		/* Only do 331 or 332 if not & 1. Set | 1 */
	} else if(!g_strcasecmp(l->args[0], "331") || !g_strcasecmp(l->args[0], "332")) {
		c = find_channel(l->network, l->args[2]);
		if(c && !(c->introduced & 1)) {
			linestack_add_line(co, l);
			c->introduced |= 1;
		}
	}

	return TRUE;
}

static gboolean simple_replicate(struct client *c, void *userdata)
{
	struct linestack_context *replication_data = (struct linestack_context *)g_hash_table_lookup(simple_backlog, c->network);
	if(replication_data) {
		char *initialnick = (char *)g_hash_table_lookup(simple_initialnick, c->network);
		change_nick(c, initialnick);
		linestack_send(replication_data, c->incoming);
	}
	return TRUE;
}

gboolean fini_plugin(struct plugin *p) {
	del_filter_ex("replicate", log_data);
	del_new_client_hook("repl_simple");
	g_hash_table_destroy(simple_backlog); simple_backlog = NULL;
	g_hash_table_destroy(simple_initialnick); simple_initialnick = NULL;
	return TRUE;
}

const char name_plugin[] = "repl_simple";

gboolean init_plugin(struct plugin *p) {
	add_filter_ex("repl_simple", log_data, NULL, "replicate", 1000);
	add_new_client_hook("repl_simple", simple_replicate, NULL);
	simple_backlog = g_hash_table_new_full(NULL, NULL, NULL, linestack_destroy);
	simple_initialnick = g_hash_table_new_full(NULL, NULL, NULL, NULL);
	return TRUE;
}
