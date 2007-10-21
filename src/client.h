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

#ifndef __CTRLPROXY_CLIENT_H__
#define __CTRLPROXY_CLIENT_H__

/**
 * @file
 * @brief Client functions
 */

#include "line.h"

#include <sys/time.h>
#include <glib.h>
#include <gmodule.h>

/**
 * Connection with a client.
 */
struct client {
	struct network *network;
	char *description;
	GIOChannel *incoming;
	GQueue *pending_lines;
	gint incoming_id;
	gint outgoing_id;
	gint ping_id;
	GIConv incoming_iconv;
	GIConv outgoing_iconv;
	time_t last_ping;
	time_t last_pong;
	time_t connect_time;
	char *requested_nick;
	char *charset;
	gboolean exit_on_close;
	gboolean connected;
	struct network_state *state;
};

/**
 * Add a new client
 *
 * @param net Network this client is connected to. May be NULL.
 * @param io IO Channel to use for communication.
 * @param desc Description of the client.
 */
G_MODULE_EXPORT G_GNUC_MALLOC struct client *client_init(
										   struct network *net, 
										   GIOChannel *io, 
										   const char *desc);

/**
 * Disconnect a client.
 */
G_MODULE_EXPORT void disconnect_client(struct client *c, const char *reason);

G_MODULE_EXPORT G_GNUC_NULL_TERMINATED gboolean client_send_args(struct client *c, ...);
G_MODULE_EXPORT G_GNUC_NULL_TERMINATED gboolean client_send_args_ex(struct client *c, 
											 const char *hm, ...);
G_MODULE_EXPORT G_GNUC_NULL_TERMINATED gboolean client_send_response(struct client *c, 
											  int response, ...);
G_MODULE_EXPORT gboolean client_send_line(struct client *c, 
										  const struct line *);
G_MODULE_EXPORT gboolean client_set_charset(struct client *c, const char *name);
G_MODULE_EXPORT const char *client_get_default_origin(struct client *c);
G_MODULE_EXPORT const char *client_get_default_target(struct client *c);
G_MODULE_EXPORT const char *client_get_own_hostmask(struct client *c);

#endif /* __CTRLPROXY_CLIENT_H__ */
