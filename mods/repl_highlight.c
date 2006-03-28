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

static GList *matches = NULL; /* FIXME: Initialize from config! */
static GHashTable *markers = NULL;

static void check_highlight(struct line *l, time_t t, void *userdata)
{
	struct client *c = userdata;
	GList *gl;

	if (strcasecmp(l->args[0], "PRIVMSG") != 0 &&
		strcasecmp(l->args[0], "NOTICE") != 0) 
		return;
	
	for (gl = matches; gl; gl = gl->next) {
		if (strstr(l->args[2], gl->data)) {
			client_send_line(c, l);
			return;
		}
	}
}

static void highlight_replicate(struct client *c)
{
	linestack_marker *lm = g_hash_table_lookup(markers, c->network);
	linestack_traverse(c->network->global->linestack, c->network, lm, NULL, check_highlight, c);
	g_hash_table_replace(markers, c->network, linestack_get_marker(c->network->global->linestack, c->network));
}

static gboolean fini_plugin(struct plugin *p) 
{
	g_hash_table_destroy(markers);	
	return TRUE;
}

static const struct replication_backend highlight = {
	.name = "highlight",
	.replication_fn = highlight_replicate
};

static gboolean init_plugin(struct plugin *p) 
{
	markers = g_hash_table_new_full(NULL, NULL, NULL, (GDestroyNotify)linestack_free_marker);
	register_replication_backend(&highlight);
	return TRUE;
}

static gboolean load_config(struct plugin *p, xmlNodePtr node)
{
	xmlNodePtr cur;
	g_list_free(matches);
	
	for (cur = node->xmlChildrenNode; cur; cur = cur->next) {
		if (!strcmp(cur->name, "match")) {
			matches = g_list_append(matches, xmlNodeGetContent(cur));
		}
	}

	return TRUE;
}

struct plugin_ops plugin = {
	.name = "repl_highlight",
	.version = 0,
	.init = init_plugin,
	.load_config = load_config,
};
