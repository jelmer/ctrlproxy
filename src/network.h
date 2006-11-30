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

#ifndef __CTRLPROXY_NETWORK_H__
#define __CTRLPROXY_NETWORK_H__

/**
 * @file
 * @brief Network functions
 */

#ifndef G_MODULE_EXPORT
#define G_MODULE_EXPORT 
#endif

#include "state.h"

struct global;
struct network;
struct client;
struct line;
struct linestack_context;

enum network_connection_state { 
		NETWORK_CONNECTION_STATE_NOT_CONNECTED = 0, 
		NETWORK_CONNECTION_STATE_RECONNECT_PENDING,
		NETWORK_CONNECTION_STATE_LOGIN_SENT, 
		NETWORK_CONNECTION_STATE_MOTD_RECVD,
};

/**
 * Information about the connection to a network.
 */
struct network_connection {
 	enum network_connection_state state;

	time_t last_line_sent;
	GQueue *pending_lines;

	GIOChannel *outgoing;
	GIOChannel *unix_incoming;
	gint outgoing_id;
	gint incoming_id;
	gint unix_incoming_id;

	union { 
		struct {
			struct tcp_server_config *current_server;
			struct sockaddr *remote_name;
			struct sockaddr *local_name;
			size_t namelen;
		} tcp;
		
		struct {
			void *private_data;
			struct virtual_network_ops {
				char *name;
				gboolean (*init) (struct network *);
				gboolean (*to_server) (struct network *, struct client *c, const struct line *);
				gboolean (*fini) (struct network *);
			} *ops;
		} virtual;
	} data;
};

/**
 * An IRC network
 */
struct network {
	char *name;
	struct global *global;
	struct network_config *config;

	GList *clients;
	guint reconnect_id;

	gpointer ssl_credentials;

	struct network_state *state;
	struct network_info info;
	struct network_connection connection;
	struct linestack_context *linestack;
};

/* server.c */
G_MODULE_EXPORT struct network *find_network_by_hostname(struct global *global, const char *host, guint16 port, gboolean create);
G_MODULE_EXPORT gboolean load_networks(struct global *, struct ctrlproxy_config *cfg);
G_MODULE_EXPORT gboolean autoconnect_networks(struct global *);
G_MODULE_EXPORT struct network *load_network(struct global *, struct network_config *);
G_MODULE_EXPORT void unload_network(struct network *);
G_MODULE_EXPORT gboolean connect_network(struct network *);
G_MODULE_EXPORT void network_select_next_server(struct network *n);
G_MODULE_EXPORT gboolean disconnect_network(struct network *s);
G_MODULE_EXPORT void clients_send(struct network *, struct line *, const struct client *exception);
G_MODULE_EXPORT gboolean network_send_line(struct network *s, struct client *c, const struct line *);
G_MODULE_EXPORT gboolean network_send_args(struct network *s, ...);
G_MODULE_EXPORT void register_virtual_network(struct virtual_network_ops *);
G_MODULE_EXPORT struct network *find_network(struct global *, const char *);
G_MODULE_EXPORT gboolean virtual_network_recv_line(struct network *l, struct line *);
G_MODULE_EXPORT gboolean virtual_network_recv_args(struct network *l, const char *origin, ...); 
typedef void (*new_network_notify_fn) (struct network *, void *);
G_MODULE_EXPORT void register_new_network_notify(struct global *, new_network_notify_fn, void *userdata);
G_MODULE_EXPORT struct linestack_context *new_linestack(struct network *network);

#endif /* __CTRLPROXY_NETWORK_H__ */
