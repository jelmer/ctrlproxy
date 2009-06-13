/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2005 Jelmer Vernooij <jelmer@nl.linux.org>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
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
struct irc_client;
#include <sys/time.h>
#include <gmodule.h>

#include "hooks.h"

struct irc_network_state;
/**
 * Mark set a specific point in time in a linestack.
 */
typedef guint64 *linestack_marker;
struct ctrlproxy_config;

/* linestack.c */
typedef gboolean (*linestack_traverse_fn) (struct irc_line *, time_t, void *);

G_MODULE_EXPORT struct irc_network_state *linestack_get_state (
		struct linestack_context *,
		linestack_marker );

G_MODULE_EXPORT gboolean linestack_traverse (
		struct linestack_context *,
		linestack_marker from,
		linestack_marker to, /* Can be NULL for 'now' */
		linestack_traverse_fn, 
		void *userdata);

G_MODULE_EXPORT gboolean linestack_traverse_object (
		struct linestack_context *,
		const char *object,
		linestack_marker from,
		linestack_marker to, /* Can be NULL for 'now' */
		linestack_traverse_fn,
		void *userdata);

G_MODULE_EXPORT gboolean linestack_send (
		struct linestack_context *,
		linestack_marker from,
		linestack_marker to, /* Can be NULL for 'now' */
		struct irc_client *, 
		gboolean dataonly, 
		gboolean timed,
		int time_offset);

G_MODULE_EXPORT gboolean linestack_send_object (
		struct linestack_context *,
		const char *object,
		linestack_marker from,
		linestack_marker to, /* Can be NULL for 'now' */
		struct irc_client *,
		gboolean dataonly,
		gboolean timed,
		int time_offset);

G_MODULE_EXPORT gboolean linestack_replay (
		struct linestack_context *,
		linestack_marker from,
		linestack_marker to,/* Can be NULL for 'now' */
		struct irc_network_state *st);

G_MODULE_EXPORT gboolean linestack_insert_line(
		struct linestack_context *, 
		const struct irc_line *l, 
		enum data_direction dir, 
		const struct irc_network_state *);

G_MODULE_EXPORT void linestack_free_marker(linestack_marker );
G_MODULE_EXPORT linestack_marker linestack_get_marker(struct linestack_context *);

/**
 * Create a new linestack context
 *
 * @param ops Linestack backend to use.
 * @param name Name of the network
 * @param cfg CtrlProxy configuration
 * @param state Current network state
 */
G_MODULE_EXPORT struct linestack_context *create_linestack(const char *name, gboolean truncate, const char *basedir, const struct irc_network_state *);
G_MODULE_EXPORT void free_linestack_context(struct linestack_context *);

G_MODULE_EXPORT gboolean linestack_read_entry(struct linestack_context *nd, 
							  guint64 i,
							  struct irc_line **line,
							  time_t *time
							 );


#endif /* __CTRLPROXY_LINESTACK_H__ */
