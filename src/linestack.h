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

/**
 * @file
 * @brief Linestack handling
 */

struct line;
struct network;
struct client;
#include <sys/time.h>
#include <gmodule.h>

#include "hooks.h"

struct network_state;
struct linestack_marker;
struct linestack_ops;
struct ctrlproxy_config;

/**
 * A linestack instance.
 */
struct linestack_context
{
	void *backend_data;
	const struct linestack_ops *ops;
};

/**
 * Mark set a specific point in time in a linestack.
 */
struct linestack_marker {
	void *data;
	void (*free_fn) (void *);
};

/* linestack.c */
typedef void (*linestack_traverse_fn) (struct line *, time_t, void *);
/**
 * Linestack functions
 */
struct linestack_ops {
	char *name;
	gboolean (*init) (struct linestack_context *, 
					  const char *name,
					  struct ctrlproxy_config *,
					  const struct network_state *
					  );
	gboolean (*fini) (struct linestack_context *);

	/* Add a line */
	gboolean (*insert_line) (
		struct linestack_context *,
		const struct line *,
		const struct network_state *);
	
	/* Get marker for current position in stream */
	void *(*get_marker) 
		(struct linestack_context *);

	/* Optional */
	void *(*get_marker_numlines) 
		(struct linestack_context *, int numlines);

	void (*free_marker) (void *);
	
	gboolean (*traverse) (
		struct linestack_context *,
		void *from,
		void *to,
		linestack_traverse_fn, 
		void *userdata);

	/* Optional */
	struct network_state *(*get_state) (
		struct linestack_context *,
		void *);

	/* Perhaps add other (optional) functions here such as searching for 
	 * a specific keyword ? */
};

G_MODULE_EXPORT void register_linestack(const struct linestack_ops *);
G_MODULE_EXPORT struct linestack_marker *linestack_get_marker_numlines (
		struct linestack_context *,
		int lines);

G_MODULE_EXPORT struct network_state *linestack_get_state (
		struct linestack_context *,
		struct linestack_marker *);

G_MODULE_EXPORT gboolean linestack_traverse (
		struct linestack_context *,
		struct linestack_marker *from,
		struct linestack_marker *to, /* Can be NULL for 'now' */
		linestack_traverse_fn, 
		void *userdata);

G_MODULE_EXPORT gboolean linestack_traverse_object (
		struct linestack_context *,
		const char *object,
		struct linestack_marker *from,
		struct linestack_marker *to, /* Can be NULL for 'now' */
		linestack_traverse_fn,
		void *userdata);

/* Same as linestack_send, but prepend all lines with [HH:MM:SS] */
G_MODULE_EXPORT gboolean linestack_send_timed (
		struct linestack_context *,
		struct linestack_marker *from,
		struct linestack_marker *to, /* Can be NULL for 'now' */
		struct client *);

G_MODULE_EXPORT gboolean linestack_send_object_timed(
		struct linestack_context *, 
		const char *obj, 
		struct linestack_marker *from, 
		struct linestack_marker *to, 
		struct client *);

G_MODULE_EXPORT gboolean linestack_send (
		struct linestack_context *,
		struct linestack_marker *from,
		struct linestack_marker *to, /* Can be NULL for 'now' */
		struct client *);

G_MODULE_EXPORT gboolean linestack_send_object (
		struct linestack_context *,
		const char *object,
		struct linestack_marker *from,
		struct linestack_marker *to, /* Can be NULL for 'now' */
		struct client *);

G_MODULE_EXPORT gboolean linestack_replay (
		struct linestack_context *,
		struct linestack_marker *from,
		struct linestack_marker *to,/* Can be NULL for 'now' */
		struct network_state *st);

G_MODULE_EXPORT gboolean linestack_insert_line(
		struct linestack_context *, 
		const struct line *l, 
		enum data_direction dir, 
		const struct network_state *);

G_MODULE_EXPORT void linestack_free_marker(struct linestack_marker *);
G_MODULE_EXPORT struct linestack_marker *linestack_get_marker(struct linestack_context *);

/**
 * Create a new linestack context
 *
 * @param ops Linestack backend to use.
 * @param name Name of the network
 * @param cfg CtrlProxy configuration
 * @param state Current network state
 */
G_MODULE_EXPORT struct linestack_context *create_linestack(const struct linestack_ops *, const char *name, struct ctrlproxy_config *, const struct network_state *);
G_MODULE_EXPORT void free_linestack_context(struct linestack_context *);
G_MODULE_EXPORT struct linestack_ops *linestack_find_ops(const char *name);

extern const struct linestack_ops linestack_file;

#endif /* __CTRLPROXY_LINESTACK_H__ */
