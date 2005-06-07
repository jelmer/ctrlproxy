/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2005 Jelmer Vernooij <jelmer@nl.linux.org>

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

#ifndef __CTRLPROXY_LINESTACK_H__
#define __CTRLPROXY_LINESTACK_H__

/* linestack.c */
struct linestack_context;
struct linestack_ops {
	char *name;
	gboolean (*init) (struct linestack_context *, const char *args);
	gboolean (*clear) (struct linestack_context *);
	gboolean (*add_line) (struct linestack_context *, struct line *);
	GSList *(*get_linked_list) (struct linestack_context *);
	void (*send) (struct linestack_context *, GIOChannel *);
	void (*send_limited) (struct linestack_context *, GIOChannel *, size_t);
	gboolean (*destroy) (struct linestack_context *);
};

struct linestack_context {
	struct linestack_ops *functions;
	void *data;
};

G_MODULE_EXPORT void register_linestack(struct linestack_ops *);
G_MODULE_EXPORT void unregister_linestack(struct linestack_ops *);
G_MODULE_EXPORT struct linestack_context *linestack_new(const char *name, const char *args);
G_MODULE_EXPORT GSList *linestack_get_linked_list(struct linestack_context *);
G_MODULE_EXPORT void linestack_send(struct linestack_context *, GIOChannel *);
G_MODULE_EXPORT gboolean linestack_destroy(struct linestack_context *);
G_MODULE_EXPORT gboolean linestack_clear(struct linestack_context *);
G_MODULE_EXPORT gboolean linestack_add_line(struct linestack_context *, struct line *);
G_MODULE_EXPORT gboolean linestack_add_line_list(struct linestack_context *, GSList *);

#endif /* __CTRLPROXY_LINESTACK_H__ */
