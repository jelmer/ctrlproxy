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

#ifndef __LIBIRC_CLIENT_H__
#define __LIBIRC_CLIENT_H__

/**
 * @file
 * @brief Client functions
 */

#include "line.h"
#include "transport.h"

#include <sys/time.h>
#include <glib.h>
#include <gmodule.h>

struct irc_client_callbacks {
	gboolean (*process_from_client) (struct irc_client *, const struct irc_line *);
	gboolean (*process_to_client) (struct irc_client *, const struct irc_line *);
	void (*disconnect) (struct irc_client *);
	void (*free_private_data) (struct irc_client *);
	void (*log_fn) (enum log_level l, const struct irc_client *, const char *data);
	gboolean (*welcome) (struct irc_client *);
};


/**
 * Connection with a client.
 */
struct irc_client {
	int references;
	struct irc_network *network;
	char *description;
	GQueue *pending_lines;
	gint ping_id;
	time_t last_ping;
	time_t last_pong;
	time_t connect_time;
	char *requested_nick;
	char *requested_username;
	char *requested_hostname;
	char *charset;
	char *default_origin;
	gboolean exit_on_close;
	gboolean connected;
	struct irc_network_state *state;
	struct irc_client_callbacks *callbacks;
	struct irc_transport *transport;
};

/**
 * Disconnect a client.
 */
G_MODULE_EXPORT void client_disconnect(struct irc_client *c, const char *reason);

G_MODULE_EXPORT G_GNUC_NULL_TERMINATED gboolean client_send_args(struct irc_client *c, ...);
G_MODULE_EXPORT G_GNUC_NULL_TERMINATED gboolean client_send_args_ex(struct irc_client *c, 
											 const char *hm, ...);
G_MODULE_EXPORT G_GNUC_NULL_TERMINATED gboolean client_send_response(struct irc_client *c, 
											  int response, ...);
G_MODULE_EXPORT gboolean client_send_line(struct irc_client *c, 
										  const struct irc_line *);
G_MODULE_EXPORT gboolean client_set_charset(struct irc_client *c, const char *name);
G_MODULE_EXPORT const char *client_get_default_target(struct irc_client *c);
G_MODULE_EXPORT const char *client_get_own_hostmask(struct irc_client *c);
G_MODULE_EXPORT struct irc_client *client_ref(struct irc_client *c);
G_MODULE_EXPORT void client_unref(struct irc_client *c);
G_MODULE_EXPORT struct irc_client *irc_client_new(GIOChannel *c, const char *default_origin, const char *desc, struct irc_client_callbacks *callbacks);
G_MODULE_EXPORT void clients_send_state(GList *clients, 
										struct irc_network_state *s);

G_MODULE_EXPORT void client_send_nameslist(struct irc_client *client, 
										   struct irc_channel_state *ch);
G_MODULE_EXPORT gboolean client_send_channel_state_diff(
										struct irc_client *client, 
										struct irc_channel_state *old_state,
										struct irc_channel_state *new_state);

G_MODULE_EXPORT gboolean client_send_state_diff(struct irc_client *client, struct irc_network_state *old_state, struct irc_network_state *new_state);

G_MODULE_EXPORT void client_send_channel_state(struct irc_client *c, 
							   struct irc_channel_state *ch);
G_MODULE_EXPORT void client_send_topic(struct irc_client *c, struct irc_channel_state *ch);
G_MODULE_EXPORT void client_send_banlist(struct irc_client *client, struct irc_channel_state *channel);
G_MODULE_EXPORT void client_send_channel_mode(struct irc_client *client, struct irc_channel_state *channel);
G_MODULE_EXPORT void client_send_luserchannels(struct irc_client *c, int num);
G_MODULE_EXPORT void client_send_motd(struct irc_client *c, char **lines);
G_MODULE_EXPORT void client_parse_buffer(struct irc_client *client);
G_MODULE_EXPORT void client_log(enum log_level, const struct irc_client *c, const char *fmt, ...);

#endif /* __LIBIRC_CLIENT_H__ */
