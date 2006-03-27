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

struct line;
struct network;
struct client;
#include <sys/time.h>
#include <gmodule.h>


struct linestack_ops;
struct linestack_context
{
	void *backend_data;
	const struct linestack_ops *ops;
};

/* linestack.c */
typedef void linestack_marker;
typedef void (*linestack_traverse_fn) (struct line *, time_t, void *);
struct linestack_ops {
	char *name;
	gboolean (*init) (struct linestack_context *, struct ctrlproxy_config *);
	gboolean (*fini) (struct linestack_context *);

	/* Add a line */
	gboolean (*insert_line) (
		struct linestack_context *,
		const struct network *, 
		const struct line *);
	
	/* Get marker for current position in stream */
	linestack_marker *(*get_marker) 
		(struct linestack_context *,
		 struct network *);

	/* Optional */
	linestack_marker *(*get_marker_numlines) 
		(struct linestack_context *, struct network *, int numlines);

	void (*free_marker) (linestack_marker *);
	
	gboolean (*traverse) (
		struct linestack_context *,
		struct network *, 
		linestack_marker *from,
		linestack_marker *to,
		linestack_traverse_fn, 
		void *userdata);

	/* Optional */
	struct network_state *(*get_state) (
		struct linestack_context *,
		struct network *, 
		linestack_marker *);

	/* Perhaps add other (optional) functions here such as searching for 
	 * a specific keyword ? */
};

G_MODULE_EXPORT void register_linestack(const struct linestack_ops *);
G_MODULE_EXPORT void unregister_linestack(const struct linestack_ops *);
G_MODULE_EXPORT linestack_marker *linestack_get_marker_numlines (
		struct linestack_context *,
		struct network *, 
		int lines);

G_MODULE_EXPORT struct network_state *linestack_get_state (
		struct linestack_context *,
		struct network *, 
		linestack_marker *);

G_MODULE_EXPORT gboolean linestack_traverse (
		struct linestack_context *,
		struct network *, 
		linestack_marker *from,
		linestack_marker *to, /* Can be NULL for 'now' */
		linestack_traverse_fn, 
		void *userdata);

G_MODULE_EXPORT gboolean linestack_traverse_object (
		struct linestack_context *,
		struct network *,
		const char *object,
		linestack_marker *from,
		linestack_marker *to, /* Can be NULL for 'now' */
		linestack_traverse_fn,
		void *userdata);

G_MODULE_EXPORT gboolean linestack_send (
		struct linestack_context *,
		struct network *,
		linestack_marker *from,
		linestack_marker *to, /* Can be NULL for 'now' */
		const struct client *);

G_MODULE_EXPORT gboolean linestack_send_object (
		struct linestack_context *,
		struct network *,
		const char *object,
		linestack_marker *from,
		linestack_marker *to, /* Can be NULL for 'now' */
		const struct client *);

G_MODULE_EXPORT gboolean linestack_replay (
		struct linestack_context *,
		struct network *,
		linestack_marker *from,
		linestack_marker *to,/* Can be NULL for 'now' */
		struct network_state *st);

G_MODULE_EXPORT void linestack_free_marker(struct linestack_context *, linestack_marker *);
G_MODULE_EXPORT linestack_marker *linestack_get_marker(struct linestack_context *, struct network *n);
G_MODULE_EXPORT struct linestack_context *new_linestack(struct ctrlproxy_config *);
G_MODULE_EXPORT void free_linestack_context(struct linestack_context *);

#endif /* __CTRLPROXY_LINESTACK_H__ */
