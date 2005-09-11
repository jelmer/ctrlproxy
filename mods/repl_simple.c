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
	struct line *l = irc_parse_line_args(c->network->state->me.hostmask, "NICK", newnick, NULL);
	client_send_line(c, l);
	free_line(l);
}

static gboolean log_data(struct network *n, struct line *l, enum data_direction dir, void *userdata) 
{
	if(dir != TO_SERVER) return TRUE;

	if (g_strcasecmp(l->args[0], "PRIVMSG") && 
		g_strcasecmp(l->args[0], "NOTICE")) return TRUE;

	g_hash_table_replace(simple_backlog, n, linestack_get_marker(n));

	return TRUE;
}

static gboolean simple_replicate(struct client *c, void *userdata)
{
	linestack_marker *m = g_hash_table_lookup(simple_backlog, c->network);
	char *initialnick = (char *)g_hash_table_lookup(simple_initialnick, c->network);
	struct network_state *ns = linestack_get_state(c->network, m);
	client_send_state(c, ns);
	free_network_state(ns);
	change_nick(c, initialnick);
	linestack_send(c->network, m, NULL, c);
	return TRUE;
}

static gboolean fini_plugin(struct plugin *p) {
	del_server_filter("repl_simple");
	del_new_client_hook("repl_simple");
	g_hash_table_destroy(simple_backlog); simple_backlog = NULL;
	g_hash_table_destroy(simple_initialnick); simple_initialnick = NULL;
	return TRUE;
}

static gboolean init_plugin(struct plugin *p) {
	add_server_filter("repl_simple", log_data, NULL, 200);
	add_new_client_hook("repl_simple", simple_replicate, NULL);
	simple_backlog = g_hash_table_new_full(NULL, NULL, NULL, linestack_free_marker);
	simple_initialnick = g_hash_table_new_full(NULL, NULL, NULL, g_free);
	return TRUE;
}

struct plugin_ops plugin = {
	.name = "repl_simple",
	.version = 0,
	.init = init_plugin,
	.fini = fini_plugin
};
