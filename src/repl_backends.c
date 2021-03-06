/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2007 Jelmer Vernooĳ <jelmer@jelmer.uk>

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

static gboolean check_highlight(struct irc_line *l, time_t t, void *userdata)
{
	struct irc_client *c = userdata;
	int i;

	if (base_strcmp(l->args[0], "PRIVMSG") != 0 &&
		base_strcmp(l->args[0], "NOTICE") != 0)
		return TRUE;

	for (i = 0; matches && matches[i]; i++) {
		if (strstr(l->args[2], matches[i]) != NULL) {
			return client_send_line(c, l, NULL);
		}
	}

	return TRUE;
}

static void highlight_replicate(struct irc_client *c)
{
	linestack_marker lm = g_hash_table_lookup(markers, c->network);

	if (c->network->external_state) {
		client_send_state(c, c->network->external_state);
	}

	if (c->network->linestack == NULL) {
		return;
	}

	linestack_traverse(c->network->linestack, lm, NULL, check_highlight, c);
	g_hash_table_replace(markers, irc_network_ref(c->network), linestack_get_marker(c->network->linestack));
}

static void none_replicate(struct irc_client *c)
{
	if (c->network->external_state)
		client_send_state(c, c->network->external_state);
}

static void lastdisconnect_mark(struct irc_client *c, void *userdata)
{
	if (!c->network)
		return;

	if (c->network->linestack != NULL)
		g_hash_table_replace(lastdisconnect_backlog, irc_network_ref(c->network),
							 linestack_get_marker(c->network->linestack));
}

static void lastdisconnect_replicate(struct irc_client *c)
{
	linestack_marker lm = g_hash_table_lookup(lastdisconnect_backlog, c->network);
	struct irc_network_state *ns;

	if (c->network->linestack == NULL)
		return;

	ns = linestack_get_state(c->network->linestack, lm);
	if (ns != NULL) {
		client_send_state(c, ns);
	}
	free_network_state(ns);

	linestack_send(c->network->linestack, lm, NULL, c, FALSE,
				   c->network->global->config->report_time != REPORT_TIME_NEVER,
				   c->network->global->config->report_time_offset);
}

static gboolean log_data(struct irc_network *n, const struct irc_line *l, enum data_direction dir, void *userdata)
{
	if(dir != TO_SERVER) return TRUE;

	if (base_strcmp(l->args[0], "PRIVMSG") &&
		base_strcmp(l->args[0], "NOTICE")) return TRUE;

	if (n->linestack != NULL)
		g_hash_table_replace(simple_backlog, irc_network_ref(n),
							 linestack_get_marker(n->linestack));

	return TRUE;
}

static void simple_replicate(struct irc_client *c)
{
	linestack_marker m;
	struct irc_network_state *ns;

	if (c->network->linestack == NULL)
		return;

	m = g_hash_table_lookup(simple_backlog, c->network);
	ns = linestack_get_state(c->network->linestack, m);
	if (ns) {
		client_send_state(c, ns);
	}
	free_network_state(ns);

	linestack_send(c->network->linestack, m, NULL, c, FALSE,
				   c->network->global->config->report_time != REPORT_TIME_NEVER,
				   c->network->global->config->report_time_offset);
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
	markers = g_hash_table_new_full(NULL, NULL,
									(GDestroyNotify)irc_network_unref,
									(GDestroyNotify)linestack_free_marker);
}

gboolean init_replication(void)
{
	int i;
	for (i = 0; backends[i].name; i++) {
		register_replication_backend(&backends[i]);
	}

	register_load_config_notify(load_config);
	add_lose_client_hook("repl_lastdisconnect", lastdisconnect_mark, NULL);
	lastdisconnect_backlog = g_hash_table_new_full(NULL, NULL,
		(GDestroyNotify)irc_network_unref,
		(GDestroyNotify)linestack_free_marker);
	simple_backlog = g_hash_table_new_full(NULL, NULL,
		(GDestroyNotify)irc_network_unref,
		(GDestroyNotify)linestack_free_marker);
	add_server_filter("repl_simple", log_data, NULL, 200);

	return TRUE;
}
