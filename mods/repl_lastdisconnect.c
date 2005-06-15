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

static void lastdisconnect_mark(struct client *c, void *userdata)
{
	g_hash_table_replace(lastdisconnect_backlog, c->network, linestack_get_marker(c->network));
}

static gboolean lastdisconnect_replicate(struct client *c, void *userdata)
{
	linestack_marker *lm = g_hash_table_lookup(lastdisconnect_backlog, c->network);
	linestack_send(c->network, lm, c);
	return TRUE;
}

static gboolean fini_plugin(struct plugin *p) {
	del_lose_client_hook("repl_lastdisconnect");
	del_new_client_hook("repl_lastdisconnect");
	g_hash_table_destroy(lastdisconnect_backlog); lastdisconnect_backlog = NULL;
	return TRUE;
}

static gboolean init_plugin(struct plugin *p) {
	add_lose_client_hook("repl_lastdisconnect", lastdisconnect_mark, NULL);
	add_new_client_hook("repl_lastdisconnect", lastdisconnect_replicate, NULL);
	lastdisconnect_backlog = g_hash_table_new_full(NULL, NULL, NULL, linestack_free_marker);
	return TRUE;
}

struct plugin_ops plugin = {
	.name = "repl_lastdisconnect",
	.version = 0,
	.init = init_plugin,
	.fini = fini_plugin
};
