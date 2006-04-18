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

static void change_nick(struct client *c, const char *newnick) 
{
	struct line *l;
	g_assert(c);
	g_assert(newnick);

	g_assert(c->network->state);
	
	l = irc_parse_line_args(c->network->state->me.hostmask, "NICK", newnick, NULL);
	client_send_line(c, l);
	free_line(l);
}

static gboolean log_data(struct network *n, const struct line *l, enum data_direction dir, void *userdata) 
{
	if(dir != TO_SERVER) return TRUE;

	if (g_strcasecmp(l->args[0], "PRIVMSG") && 
		g_strcasecmp(l->args[0], "NOTICE")) return TRUE;

	g_hash_table_replace(simple_backlog, n, linestack_get_marker(n->global->linestack, n));

	return TRUE;
}

static void simple_replicate(struct client *c)
{
	struct linestack_marker *m;
	struct network_state *ns;

	m = g_hash_table_lookup(simple_backlog, c->network);
	ns = linestack_get_state(c->network->global->linestack, c->network, m);
	if (ns) {
		client_send_state(c, ns);
		change_nick(c, ns->me.nick);
	}
	free_network_state(ns);

	if (c->network->global->config->report_time)
		linestack_send_timed(c->network->global->linestack, c->network, m, NULL, c);
	else
		linestack_send(c->network->global->linestack, c->network, m, NULL, c);
}

static const struct replication_backend simple = 
{
	.name = "simple",
	.replication_fn = simple_replicate
};

static gboolean init_plugin(void)
{
	add_server_filter("repl_simple", log_data, NULL, 200);
	register_replication_backend(&simple);
	simple_backlog = g_hash_table_new_full(NULL, NULL, NULL, (void (*)(void *))linestack_free_marker);
	return TRUE;
}

struct plugin_ops plugin = {
	.name = "repl_simple",
	.version = 0,
	.init = init_plugin,
};
