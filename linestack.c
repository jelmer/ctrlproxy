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

#include "internals.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

GSList *linestack_backends = NULL;

gboolean linestack_clear(struct linestack_context *p)
{
	if(!p) return FALSE;
	if(!p->functions->clear) return FALSE;
	return p->functions->clear(p);
}

gboolean linestack_add_line(struct linestack_context *p, struct line *l)
{
	if(!p->functions->add_line) return FALSE;
	return p->functions->add_line(p, l);
}

gboolean linestack_add_line_list(struct linestack_context *p, GSList *l)
{
	GSList *gl = l;
	while(gl) {
		struct line *li = (struct line *)gl->data;
		linestack_add_line(p, li);
		gl = gl->next;
	}
	return TRUE;
}

struct linestack_context *linestack_new(char *name, char *args)
{
	struct linestack_ops *b = NULL;
	struct linestack_context *c = NULL;
	GSList *gl = linestack_backends;
	while(gl) {
		struct linestack_ops *b1 = (struct linestack_ops *)gl->data;
		if(!name || !strcmp(b1->name, name)) b = b1;
		gl = gl->next;
	}

	if(!b) return NULL;

	c = malloc(sizeof(struct linestack_context));
	c->functions = b;
	c->data = NULL;
	if(!b->init(c, args)) { 
		free(c);
		return NULL;
	}

	return c;
}

GSList *linestack_get_linked_list(struct linestack_context *b)
{
	if(!b->functions->get_linked_list) return FALSE;

	return b->functions->get_linked_list(b);
}

void linestack_send(struct linestack_context *b, struct transport_context *t)
{
	GSList *lines, *gl;
	if(b->functions->send) b->functions->send(b, t);

	/* Fallback for if the backend doesn't implement this function */

	lines = linestack_get_linked_list(b);
	gl = lines;
	while(gl) {
		struct line *l = (struct line *)gl->data;
		char *raw = irc_line_string_nl(l);
		transport_write(t, raw);
		free(raw);
		free_line(l);
		gl = gl->next;
	}
	g_slist_free(lines);
}

gboolean linestack_destroy(struct linestack_context *b) 
{
	if(b->functions->destroy)b->functions->destroy(b);
	free(b);
	return TRUE;
}

void register_linestack(struct linestack_ops *b)
{
	g_message("Added linestack backend '%s'", b->name);
	linestack_backends = g_slist_append(linestack_backends, b);
}

void unregister_linestack(struct linestack_ops *b)
{
	g_message("Removed linestack backend '%s'", b->name);
	linestack_backends = g_slist_remove(linestack_backends, b);
}
