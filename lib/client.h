/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2008 Jelmer Vernooij <jelmer@nl.linux.org>

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
struct irc_client {
	int references;
	struct irc_network *network;
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
	char *requested_username;
	char *requested_hostname;
	char *charset;
	gboolean exit_on_close;
	gboolean connected;
	struct network_state *state;
	gboolean (*process_from_client) (struct irc_client *, const struct irc_line *);
};

/**
 * Disconnect a client.
 */
G_MODULE_EXPORT void disconnect_client(struct irc_client *c, const char *reason);

G_MODULE_EXPORT G_GNUC_NULL_TERMINATED gboolean client_send_args(struct irc_client *c, ...);
G_MODULE_EXPORT G_GNUC_NULL_TERMINATED gboolean client_send_args_ex(struct irc_client *c, 
											 const char *hm, ...);
G_MODULE_EXPORT G_GNUC_NULL_TERMINATED gboolean client_send_response(struct irc_client *c, 
											  int response, ...);
G_MODULE_EXPORT gboolean client_send_line(struct irc_client *c, 
										  const struct irc_line *);
G_MODULE_EXPORT gboolean client_set_charset(struct irc_client *c, const char *name);
G_MODULE_EXPORT const char *client_get_default_origin(struct irc_client *c);
G_MODULE_EXPORT const char *client_get_default_target(struct irc_client *c);
G_MODULE_EXPORT const char *client_get_own_hostmask(struct irc_client *c);
G_MODULE_EXPORT struct irc_client *client_ref(struct irc_client *c);
G_MODULE_EXPORT void client_unref(struct irc_client *c);
G_MODULE_EXPORT struct irc_client *irc_client_new(GIOChannel *c, const char *desc, gboolean (*process_from_client) (struct irc_client *, const struct irc_line *), struct irc_network *n);
G_MODULE_EXPORT void clients_send(GList *clients, const struct irc_line *, const struct irc_client *exception);
G_MODULE_EXPORT void clients_send_args_ex(GList *clients, const char *hostmask, ...);

#endif /* __CTRLPROXY_CLIENT_H__ */
