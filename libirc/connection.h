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

#ifndef __CTRLPROXY_NETWORK_H__
#define __CTRLPROXY_NETWORK_H__

/**
 * @file
 * @brief Network functions
 */

#include <glib.h>

#ifndef G_MODULE_EXPORT
#define G_MODULE_EXPORT 
#endif

#include "state.h"
#include "isupport.h"
#include <sys/socket.h>

#define DEFAULT_NETWORK_CHARSET NULL

struct global;
struct irc_network;
struct irc_client;
struct irc_line;
struct linestack_context;

enum irc_network_connection_state { 
		NETWORK_CONNECTION_STATE_NOT_CONNECTED = 0, 
		NETWORK_CONNECTION_STATE_RECONNECT_PENDING,
		NETWORK_CONNECTION_STATE_CONNECTING,
		NETWORK_CONNECTION_STATE_CONNECTED,
		NETWORK_CONNECTION_STATE_LOGIN_SENT, 
		NETWORK_CONNECTION_STATE_MOTD_RECVD,
};

/**
 * Information about the connection to a network.
 */
struct irc_network_connection {
 	enum irc_network_connection_state state;

	/** Time the last line was sent to this network. */
	time_t last_line_sent;
	/** Time the last line was received from this network. */
	time_t last_line_recvd;

	struct irc_transport *transport;
	union { 
		struct {
			/** Configuration for TCP/IP server currently connected to. */
			struct tcp_server_config *current_server;
			/** Name of remote server. */
			struct sockaddr *remote_name;
			/** Name of local host used for connection. */
			struct sockaddr *local_name;
			/** Socket name length for remote_name and local_name. */
			socklen_t namelen;
			/** Last reason for disconnect. */
			char *last_disconnect_reason;
			/** Source ID for function that regularly pings the network. */
			gint ping_id;
			/** Source ID for function that finishes connect. */
			gint connect_id;
		} tcp;
		
		struct {
			void *private_data;
			struct virtual_network_ops {
				char *name;
				gboolean not_disconnectable;
				gboolean (*init) (struct irc_network *);
				gboolean (*to_server) (struct irc_network *, struct irc_client *c, const struct irc_line *);
				void (*fini) (struct irc_network *);
			} *ops;
		} virtual;
	} data;
};

typedef void (*network_log_fn) (enum log_level, const struct irc_network *, const char *);

struct irc_network_callbacks {
	network_log_fn log;
	struct irc_login_details *(*get_login_details) (struct irc_network *);
	gboolean (*process_from_server) (struct irc_network *, const struct irc_line *);
	void (*disconnect) (struct irc_network *);
};

/**
 * An IRC network
 */
struct irc_network {
	char *name;

	/** Global data. */
	struct global *global;

	/** Private data */
	void *private_data;

	/** Number of references that exist to this network. */
	int references;

	/** Clients connected to this network.*/
	GList *clients;
	guint reconnect_id;

	gpointer ssl_credentials;

	int reconnect_interval;

	/** External network state, when connected. */
	struct irc_network_state *external_state;

	/** Internal network state, as used by clients */
	struct irc_network_state *internal_state;

	/** Network information. */
	struct irc_network_info *info;

	struct irc_network_connection connection;
	
	/** Linestack context. */
	struct linestack_context *linestack;

	/** How many linestack errors have occurred so far */
	guint linestack_errors;

	const struct irc_network_callbacks *callbacks;

	struct query_stack *queries;
};

/* server.c */
G_MODULE_EXPORT gboolean network_set_charset(struct irc_network *n, const char *name);
G_MODULE_EXPORT gboolean autoconnect_networks(GList *);
G_MODULE_EXPORT struct irc_network *irc_network_new(const struct irc_network_callbacks *callbacks, void *private_data);
G_MODULE_EXPORT gboolean connect_network(struct irc_network *);
G_MODULE_EXPORT void irc_network_select_next_server(struct irc_network *n);
G_MODULE_EXPORT gboolean disconnect_network(struct irc_network *s);
G_MODULE_EXPORT gboolean network_send_line(struct irc_network *s, struct irc_client *c, const struct irc_line *, gboolean);
G_MODULE_EXPORT gboolean network_send_args(struct irc_network *s, ...);
G_MODULE_EXPORT void register_virtual_network(struct virtual_network_ops *);
G_MODULE_EXPORT struct virtual_network_ops *find_virtual_network(const char *name);
G_MODULE_EXPORT struct irc_network *find_network(GList *gl, const char *);
G_MODULE_EXPORT gboolean virtual_network_recv_line(struct irc_network *l, struct irc_line *);
G_MODULE_EXPORT gboolean virtual_network_recv_args(struct irc_network *l, const char *origin, ...); 
G_MODULE_EXPORT gboolean virtual_network_recv_response(struct irc_network *n, int num, ...);
struct ctrlproxy_config;
G_MODULE_EXPORT G_GNUC_MALLOC struct linestack_context *new_linestack(struct irc_network *network, struct ctrlproxy_config *settings);
G_MODULE_EXPORT G_GNUC_MALLOC char *network_generate_feature_string(struct irc_network *n);
G_MODULE_EXPORT struct irc_network *network_ref(struct irc_network *);
G_MODULE_EXPORT void irc_network_unref(struct irc_network *);
G_MODULE_EXPORT void unregister_virtual_networks(void);

#endif /* __CTRLPROXY_NETWORK_H__ */
