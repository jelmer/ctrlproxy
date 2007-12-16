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
struct client;
struct line;
struct linestack_context;

enum network_connection_state { 
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
struct network_connection {
 	enum network_connection_state state;

	/** Time the last line was sent to this network. */
	time_t last_line_sent;
	/** Time the last line was received from this network. */
	time_t last_line_recvd;
	/** Lines that are pending to be sent. */
	GQueue *pending_lines;

	/** IO Channel. */
	GIOChannel *outgoing;
	gint outgoing_id;
	gint incoming_id;
	GIConv outgoing_iconv;
	GIConv incoming_iconv;

	union { 
		struct {
			struct tcp_server_config *current_server;
			struct sockaddr *remote_name;
			struct sockaddr *local_name;
			socklen_t namelen;
			char *last_disconnect_reason;
			gint ping_id;
		} tcp;
		
		struct {
			void *private_data;
			struct virtual_network_ops {
				char *name;
				gboolean not_disconnectable;
				gboolean (*init) (struct irc_network *);
				gboolean (*to_server) (struct irc_network *, struct client *c, const struct line *);
				void (*fini) (struct irc_network *);
			} *ops;
		} virtual;
	} data;
};

typedef void (*network_log_fn) (enum log_level, void *, const char *);

/**
 * An IRC network
 */
struct irc_network {
	struct global *global;

	/** Configuration. */
	struct network_config *config;

	/** Number of references that exist to this network. */
	int references;

	/** Clients connected to this network.*/
	GList *clients;
	guint reconnect_id;

	gpointer ssl_credentials;

	/** Current network state, when connected. */
	struct network_state *state;

	/** Network information. */
	struct network_info info;
	struct network_connection connection;
	
	/** Linestack context. */
	struct linestack_context *linestack;

	void *userdata;
	network_log_fn log;

	int reconnect_interval;

	gboolean (*process_from_server) (struct irc_network *, const struct line *);
};

/* server.c */
G_MODULE_EXPORT gboolean network_set_charset(struct irc_network *n, const char *name);
G_MODULE_EXPORT struct irc_network *find_network_by_hostname(struct global *global, const char *host, guint16 port, gboolean create);
G_MODULE_EXPORT gboolean load_networks(struct global *, struct ctrlproxy_config *cfg, network_log_fn);
G_MODULE_EXPORT gboolean autoconnect_networks(GList *);
G_MODULE_EXPORT struct irc_network *load_network(struct global *, struct network_config *);
G_MODULE_EXPORT void unload_network(struct irc_network *);
G_MODULE_EXPORT gboolean connect_network(struct irc_network *);
G_MODULE_EXPORT void network_select_next_server(struct irc_network *n);
G_MODULE_EXPORT gboolean disconnect_network(struct irc_network *s);
G_MODULE_EXPORT void clients_send(GList *clients, const struct line *, const struct client *exception);
G_MODULE_EXPORT void clients_send_args_ex(GList *clients, const char *hostmask, ...);
G_MODULE_EXPORT gboolean network_send_line(struct irc_network *s, struct client *c, const struct line *, gboolean);
G_MODULE_EXPORT gboolean network_send_args(struct irc_network *s, ...);
G_MODULE_EXPORT void register_virtual_network(struct virtual_network_ops *);
G_MODULE_EXPORT struct irc_network *find_network(GList *gl, const char *);
G_MODULE_EXPORT gboolean virtual_network_recv_line(struct irc_network *l, struct line *);
G_MODULE_EXPORT gboolean virtual_network_recv_args(struct irc_network *l, const char *origin, ...); 
G_MODULE_EXPORT gboolean virtual_network_recv_response(struct irc_network *n, int num, ...);
typedef void (*new_network_notify_fn) (struct irc_network *, void *);
G_MODULE_EXPORT void register_new_network_notify(struct global *, new_network_notify_fn, void *userdata);
G_MODULE_EXPORT G_GNUC_MALLOC struct linestack_context *new_linestack(struct irc_network *network);
G_MODULE_EXPORT G_GNUC_MALLOC char *network_generate_feature_string(struct irc_network *n);
G_MODULE_EXPORT struct irc_network *network_ref(struct irc_network *);
G_MODULE_EXPORT void network_unref(struct irc_network *);
G_MODULE_EXPORT void network_set_log_fn(struct irc_network *s, 
										network_log_fn, void *userdata);

#endif /* __CTRLPROXY_NETWORK_H__ */
