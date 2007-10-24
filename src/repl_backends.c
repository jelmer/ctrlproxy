/* 
	ctrlproxy: A modular IRC proxy
	(c) 2002-2007 Jelmer Vernooij <jelmer@nl.linux.org>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
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

static char **matches = NULL;
static GHashTable *markers = NULL;
static GHashTable *lastdisconnect_backlog = NULL;
static GHashTable *simple_backlog = NULL;

static gboolean check_highlight(struct line *l, time_t t, void *userdata)
{
	struct client *c = userdata;
    int i;

	if (g_strcasecmp(l->args[0], "PRIVMSG") != 0 &&
		g_strcasecmp(l->args[0], "NOTICE") != 0) 
		return TRUE;
	
	for (i = 0; matches && matches[i]; i++) {
		if (strstr(l->args[2], matches[i]) != NULL) {
			return client_send_line(c, l);
		}
	}

	return TRUE;
}

static void highlight_replicate(struct client *c)
{
	struct linestack_marker *lm = g_hash_table_lookup(markers, c->network);

	if (c->network->state) {
		client_send_state(c, c->network->state);
	}

	if (c->network->linestack == NULL)
		return;

	linestack_traverse(c->network->linestack, lm, NULL, check_highlight, c);
	g_hash_table_replace(markers, c->network, linestack_get_marker(c->network->linestack));
}

static void none_replicate(struct client *c)
{
	if (c->network->state)
		client_send_state(c, c->network->state);
}

static void lastdisconnect_mark(struct client *c, void *userdata)
{
	if (!c->network)
		return;

	if (c->network->linestack != NULL) 
		g_hash_table_replace(lastdisconnect_backlog, c->network, 
							 linestack_get_marker(c->network->linestack));
}

static void lastdisconnect_replicate(struct client *c)
{
	struct linestack_marker *lm = g_hash_table_lookup(lastdisconnect_backlog, c->network);
	struct network_state *ns;

	if (c->network->linestack == NULL)
		return;

	ns = linestack_get_state(c->network->linestack, lm);
	if (ns) {
		client_send_state(c, ns);
	}
	free_network_state(ns);

	linestack_send(c->network->linestack, lm, NULL, c, TRUE, 
				   c->network->global->config->report_time);
}

static gboolean log_data(struct network *n, const struct line *l, enum data_direction dir, void *userdata) 
{
	if(dir != TO_SERVER) return TRUE;

	if (g_strcasecmp(l->args[0], "PRIVMSG") && 
		g_strcasecmp(l->args[0], "NOTICE")) return TRUE;

	if (n->linestack != NULL) 
		g_hash_table_replace(simple_backlog, n, linestack_get_marker(n->linestack));

	return TRUE;
}

static void simple_replicate(struct client *c)
{
	struct linestack_marker *m;
	struct network_state *ns;

	if (c->network->linestack == NULL)
		return;

	m = g_hash_table_lookup(simple_backlog, c->network);
	ns = linestack_get_state(c->network->linestack, m);
	if (ns) {
		client_send_state(c, ns);
	}
	free_network_state(ns);

	linestack_send(c->network->linestack, m, NULL, c, FALSE, 
				   c->network->global->config->report_time);
}

static const struct replication_backend backends[] = {
	{ "none", none_replicate },
	{ "highlight", highlight_replicate },
	{ "lastdisconnect", lastdisconnect_replicate },
	{ "simple", simple_replicate },
	{ NULL }
};

static void load_config(struct global *global)
{
    matches = g_key_file_get_string_list(global->config->keyfile,
                           "global", "match", NULL, NULL);
	markers = g_hash_table_new_full(NULL, NULL, NULL, (GDestroyNotify)linestack_free_marker);
}

gboolean init_replication(void)
{
	int i;
	for (i = 0; backends[i].name; i++) 
		register_replication_backend(&backends[i]);

	register_load_config_notify(load_config);
	add_lose_client_hook("repl_lastdisconnect", lastdisconnect_mark, NULL);
	lastdisconnect_backlog = g_hash_table_new_full(NULL, NULL, NULL, (GDestroyNotify)linestack_free_marker);
	simple_backlog = g_hash_table_new_full(NULL, NULL, NULL, (GDestroyNotify)linestack_free_marker);
	add_server_filter("repl_simple", log_data, NULL, 200);

	return TRUE;
}
