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

#include <ctrlproxy.h>
#include <string.h>

static GHashTable *networks = NULL;

struct memory_data {
	struct line *line;
	time_t time;
};

static gboolean memory_init(void)
{
	networks = g_hash_table_new(NULL, NULL);
	return TRUE;
}

static gboolean memory_add_line(const struct network *n, const struct line *l)
{
	GList *gl = g_hash_table_lookup(networks, n);
	struct memory_data *md = g_new0(struct memory_data, 1);
	md->line = linedup(l);
	md->time = time(NULL);
	gl = g_list_append(gl, md);
	g_hash_table_replace(networks, n, gl);
	return TRUE;
}

static linestack_marker *memory_get_marker (struct network *n)
{
	return g_hash_table_lookup(networks, n);
}

static gboolean memory_traverse (struct network *n, linestack_marker *lm, linestack_traverse_fn tf, void *userdata)
{
	GList *gl;

	for (gl = lm; gl; gl = gl->next) {
		struct memory_data *md = gl->data;
		tf(md->line, md->time, userdata);
	}

	return TRUE;
}

static struct linestack_ops memory = {
	.name = "memory",
	.init = memory_init,
	.insert_line = memory_add_line,
	.get_marker = memory_get_marker,
	.traverse = memory_traverse,
};

static gboolean fini_plugin(struct plugin *p) {
	unregister_linestack(&memory);
	return TRUE;
}

static gboolean init_plugin(struct plugin *p) {
	register_linestack(&memory);
	return TRUE;
}

struct plugin_ops plugin = {
	.name = "linestack_memory",
	.version = 0,
	.init = init_plugin,
	.fini = fini_plugin
};
