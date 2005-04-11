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
	if(!p || !l) return FALSE;
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

struct linestack_context *linestack_new(const char *name, const char *args)
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

	c = g_new(struct linestack_context, 1);
	c->functions = b;
	c->data = NULL;
	if(!b->init(c, args)) { 
		g_free(c);
		return NULL;
	}

	return c;
}

GSList *linestack_get_linked_list(struct linestack_context *b)
{
	if(!b->functions->get_linked_list) return FALSE;

	return b->functions->get_linked_list(b);
}

void linestack_send_limited(struct linestack_context *b, GIOChannel *t, size_t last)
{
	GSList *lines, *gl;
	unsigned int i;
	if(!b) return;
	if(b->functions->send_limited) { 
		b->functions->send_limited(b, t, last);
		return;
	}

	/* Fallback for if the backend doesn't implement this function */

	lines = linestack_get_linked_list(b);
	gl = g_slist_last(lines);
	i = g_slist_position(gl, lines);
	gl = g_slist_nth(lines, (last > i)?0:(i-last));

	while(gl) {
		struct line *l = (struct line *)gl->data;
		irc_send_line(t, l);
		free_line(l);
		gl = gl->next;
	}
	g_slist_free(lines);
}

void linestack_send(struct linestack_context *b, GIOChannel *t)
{
	GSList *lines, *gl;
	if(!b) return;
	if(b->functions->send) {
		b->functions->send(b, t);
		return;
	}

	/* Fallback for if the backend doesn't implement this function */

	lines = linestack_get_linked_list(b);
	gl = lines;
	while(gl) {
		struct line *l = (struct line *)gl->data;
		irc_send_line(t, l);
		free_line(l);
		gl = gl->next;
	}
	g_slist_free(lines);
}

gboolean linestack_destroy(struct linestack_context *b) 
{
	if(b->functions->destroy)b->functions->destroy(b);
	g_free(b);
	return TRUE;
}

void register_linestack(struct linestack_ops *b)
{
	linestack_backends = g_slist_append(linestack_backends, b);
}

void unregister_linestack(struct linestack_ops *b)
{
	linestack_backends = g_slist_remove(linestack_backends, b);
}
