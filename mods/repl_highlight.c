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

static char **matches = NULL;
static GHashTable *markers = NULL;

static void check_highlight(struct line *l, time_t t, void *userdata)
{
	struct client *c = userdata;
    int i;

	if (g_strcasecmp(l->args[0], "PRIVMSG") != 0 &&
		g_strcasecmp(l->args[0], "NOTICE") != 0) 
		return;
	
	for (i = 0; matches && matches[i]; i++) {
		if (strstr(l->args[2], matches[i])) {
			client_send_line(c, l);
			return;
		}
	}
}

static void highlight_replicate(struct client *c)
{
	struct linestack_marker *lm = g_hash_table_lookup(markers, c->network);
	linestack_traverse(c->network->linestack, lm, NULL, check_highlight, c);
	g_hash_table_replace(markers, c->network, linestack_get_marker(c->network->linestack));
}

static const struct replication_backend highlight = {
	.name = "highlight",
	.replication_fn = highlight_replicate
};

static void load_config(struct global *global)
{
    matches = g_key_file_get_string_list(global->config->keyfile,
                           "global", "match", NULL, NULL);
	markers = g_hash_table_new_full(NULL, NULL, NULL, (GDestroyNotify)linestack_free_marker);
}

static gboolean init_plugin(void)
{
	register_replication_backend(&highlight);
	register_load_config_notify(load_config);
	return TRUE;
}

struct plugin_ops plugin = {
	.name = "repl_highlight",
	.version = 0,
	.init = init_plugin,
};
