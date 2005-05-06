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

static gboolean memory_init(struct linestack_context *c, const char *args)
{
	c->data = NULL;
	return TRUE;
}

static gboolean memory_clear(struct linestack_context *c)
{
	GSList *gl = (GSList *)c->data;
	while(gl) {
		struct line *l = (struct line *)gl->data;
		free_line(l);
		gl = g_slist_next(gl);
	}
	g_slist_free(c->data); c->data = NULL;
	return TRUE;
}

static GSList *memory_get_linked_list(struct linestack_context *c)
{
	GSList *b, *ret = NULL;
	b = (GSList *)c->data;
	while(b) {
		struct line *l = linedup((struct line *)b->data);
		ret = g_slist_append(ret, l);
		b = b->next;
	}
	
	return ret;
}

static gboolean memory_add_line(struct linestack_context *b, struct line *l)
{
	GSList *gl = (GSList *)b->data;
	gl = g_slist_append(gl, linedup(l));
	b->data = gl;
	return TRUE;
}	

static struct linestack_ops memory = {
	"memory",
	memory_init,
	memory_clear,
	memory_add_line,
	memory_get_linked_list,
	NULL, 
	NULL,
	memory_clear
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
