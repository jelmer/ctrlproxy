/*
	ctrlproxy: A modular IRC proxy
	(c) 2002 Jelmer Vernooij <jelmer@nl.linux.org>

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

#ifndef __CTRLPROXY_H__
#define __CTRLPROXY_H__

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <glib.h>
#include <gmodule.h>

#include <libxml/tree.h>

#ifdef STRICT_MEMORY_ALLOCS
#define calloc(a,b) __ERROR_USE_G_NEW0__
#define malloc(a) __ERROR_USE_G_MALLOC_OR_G_NEW__
#define realloc(a,b) __ERROR_USE_G_REALLOC_OR_G_RE_NEW__
#define free(a) __ERROR_USE_G_FREE__
#define strdup(a) __ERROR_USE_G_STRDUP__
#define strndup(a) __ERROR_USE_G_STRNDUP__
#endif

#ifndef G_MODULE_EXPORT
#define G_MODULE_EXPORT 
#endif

struct network;
struct client;
struct line;
struct channel_nick;
struct network_nick;
struct plugin;

enum data_direction { UNKNOWN = 0, TO_SERVER = 1, FROM_SERVER = 2 };
enum has_colon { COLON_UNKNOWN = 0, WITH_COLON = 1, WITHOUT_COLON = 2 };

/* for the options fields */
enum line_options {
	LINE_IS_PRIVATE = 1,
	LINE_DONT_SEND = 2,
	LINE_STOP_PROCESSING = 4,
	LINE_NO_LOGGING = 8
};

struct line {
	enum data_direction direction;
	enum line_options options;
	struct network *network;
	struct client *client;
	char *origin;
	char **args; /* NULL terminated */
	size_t argc;
	/* Don't use the following properties. Use line_get_network_nick() 
	 * instead. */
	struct network_nick *network_nick; 
	enum has_colon has_endcolon;
};

struct channel_nick {
	char mode;
	struct channel *channel;
	struct network_nick *global_nick;
};

struct network_nick {
	guint refcount;
	char *name;
	char *hostmask;
	GList *channels;
};

struct channel {
	char *name;
	char *key;
	char *topic;
	gboolean autojoin;
	gboolean joined;
	char mode; /* Private, secret, etc */
	char modes[255];
	char introduced;
	gboolean namreply_started;
	long limit;
	GList *nicks;
	struct network *network;
};

struct client {
	struct network *network;
	char *description;
	GIOChannel *incoming;
	gint incoming_id;
	time_t connect_time;
	char* nick;
	char *fullname;
	char *username;
};

enum casemapping { CASEMAP_UNKNOWN = 0, CASEMAP_RFC1459, CASEMAP_ASCII, CASEMAP_STRICT_RFC1459 };
enum network_type { NETWORK_TCP, NETWORK_PROGRAM, NETWORK_VIRTUAL };

struct network {
	char *name;
	char *username;
	char *fullname;
	char *nick;
	char *hostmask;
	char *password;
	gboolean autoconnect;
	gboolean ignore_first_nick;
	char mymodes[255];
	GList *channels;
	GList *nicks;
	gboolean login_sent;
	gboolean authenticated;
	GList *clients;
	GList *autosend_lines;
	GHashTable *server_features;
	guint reconnect_id;
	gboolean name_guessed;
	guint reconnect_interval;
	char *supported_modes[2];
	enum network_type type;

	struct {
		enum casemapping casemapping;
		int channellen;
		int nicklen;
		int topiclen;
	} supports;

	union { 
		struct {
			GList *servers;
			struct tcp_server {
				char *name;
				char *host;
				char *port;
				gboolean ssl;
				char *password;
				struct addrinfo *addrinfo;
			} *current_server;

			struct sockaddr *local_name;
			size_t namelen;
			GIOChannel *outgoing;
			gint outgoing_id;
		} tcp;
		
		struct {
			char *location;
			GIOChannel *outgoing;
			gint outgoing_id;
		} program;

		struct {
			char *type;
			void *private_data;
			struct virtual_network_ops {
				char *name;
				gboolean (*init) (struct network *);
				gboolean (*to_server) (struct network *, struct line *);
				gboolean (*fini) (struct network *);
			} *ops;
		} virtual;
	} connection;
};

typedef gboolean (*plugin_init_function) (struct plugin *);
typedef gboolean (*plugin_fini_function) (struct plugin *);
typedef gboolean (*plugin_save_function) (struct plugin *, xmlNodePtr);
typedef gboolean (*plugin_load_function) (struct plugin *, xmlNodePtr configuration);

struct plugin {
	char *name;
	char *path;
	GModule *module;
	void *data;
	gboolean autoload;
	gboolean loaded;
	plugin_save_function save_config;
	plugin_load_function load_config;
};

/* state.c */
G_MODULE_EXPORT struct channel *find_channel(struct network *st, const char *name);
G_MODULE_EXPORT struct channel_nick *find_nick(struct channel *c, const char *name);
G_MODULE_EXPORT struct linestack_context *linestack_new_by_network(struct network *);
G_MODULE_EXPORT GSList *gen_replication_network(struct network *s);
G_MODULE_EXPORT GSList *gen_replication_channel(struct channel *c, const char *hostmask, const char *nick);
G_MODULE_EXPORT int is_channelname(const char *name, struct network *s);
G_MODULE_EXPORT int is_prefix(char p, struct network *n);
G_MODULE_EXPORT char get_prefix_by_mode(char p, struct network *n);
G_MODULE_EXPORT const char *get_network_feature(struct network *n, const char *name);
G_MODULE_EXPORT int irccmp(struct network *n, const char *a, const char *b);
G_MODULE_EXPORT struct network_nick *line_get_network_nick(struct line *l);

/* server.c */
G_MODULE_EXPORT gboolean network_is_connected(struct network *);
G_MODULE_EXPORT struct network *find_network_by_hostname(const char *host, guint16 port, gboolean create);
G_MODULE_EXPORT struct network *new_network(void);
G_MODULE_EXPORT gboolean connect_network(struct network *);
G_MODULE_EXPORT gboolean connect_current_tcp_server (struct network *);
G_MODULE_EXPORT gboolean connect_next_tcp_server (struct network *);
G_MODULE_EXPORT int close_network(struct network *s);
G_MODULE_EXPORT gboolean close_server(struct network *n);
G_MODULE_EXPORT GList *get_network_list(void);
G_MODULE_EXPORT void clients_send(struct network *, struct line *, struct client *exception);
G_MODULE_EXPORT void disconnect_client(struct client *c);
G_MODULE_EXPORT gboolean network_change_nick(struct network *s, const char *nick);
G_MODULE_EXPORT struct client *new_client(struct network *, GIOChannel *);
G_MODULE_EXPORT gboolean network_send_line(struct network *s, struct line *);
G_MODULE_EXPORT gboolean network_send_args(struct network *s, ...);
G_MODULE_EXPORT void register_virtual_network(struct virtual_network_ops *);
G_MODULE_EXPORT void unregister_virtual_network(struct virtual_network_ops *);
G_MODULE_EXPORT struct network *find_network(const char *name);
G_MODULE_EXPORT gboolean client_send_line(struct client *c, struct line *);
G_MODULE_EXPORT gboolean virtual_network_recv_line(struct network *l, struct line *);
G_MODULE_EXPORT gboolean virtual_network_recv_args(struct network *l, const char *origin, ...); 

/* line.c */
G_MODULE_EXPORT struct line *linedup(struct line *l);
G_MODULE_EXPORT struct line * irc_parse_line(const char *data);
G_MODULE_EXPORT struct line * virc_parse_line(const char *origin, va_list ap);
G_MODULE_EXPORT char *irc_line_string(struct line *l);
G_MODULE_EXPORT char *irc_line_string_nl(struct line *l);
G_MODULE_EXPORT char *line_get_nick(struct line *l);
G_MODULE_EXPORT void free_line(struct line *l);
G_MODULE_EXPORT gboolean irc_send_args(GIOChannel *, ...);
G_MODULE_EXPORT gboolean irc_sendf(GIOChannel *, char *fmt, ...);
G_MODULE_EXPORT int irc_send_line(GIOChannel *, struct line *l);
G_MODULE_EXPORT struct line *irc_parse_linef( char *origin, ... );
G_MODULE_EXPORT struct line *irc_parse_line_args( char *origin, ... );
G_MODULE_EXPORT struct line *irc_recv_line(GIOChannel *c);

/* main.c */
G_MODULE_EXPORT const char *ctrlproxy_version(void);
G_MODULE_EXPORT const char *get_my_hostname(void);
G_MODULE_EXPORT const char *get_modules_path(void);
G_MODULE_EXPORT const char *get_shared_path(void);

/* config.c */
G_MODULE_EXPORT void save_configuration(const char *name);
G_MODULE_EXPORT gboolean load_configuration(const char *name);

/* plugins.c */
G_MODULE_EXPORT struct plugin *new_plugin(const char *name);
G_MODULE_EXPORT gboolean load_plugin(struct plugin *);
G_MODULE_EXPORT gboolean unload_plugin(struct plugin *);
G_MODULE_EXPORT gboolean plugin_loaded(char *name);
G_MODULE_EXPORT GList *get_plugin_list(void);

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

/* util.c */
G_MODULE_EXPORT char *list_make_string(GList *);
G_MODULE_EXPORT int verify_client(struct network *s, struct client *c);
G_MODULE_EXPORT char *ctrlproxy_path(char *part);
G_MODULE_EXPORT int strrfc1459cmp(const char *a, const char *b);

/* hooks.c */
/* Returns TRUE if filter should be continued, FALSE if it should be stopped. */
typedef gboolean (*filter_function) (struct line *, void *userdata);
G_MODULE_EXPORT void add_log_filter(const char *name, filter_function, void *userdata, int priority);
G_MODULE_EXPORT void del_log_filter(const char *name);

G_MODULE_EXPORT void add_replication_filter(const char *name, filter_function, void *userdata, int priority);
G_MODULE_EXPORT void del_replication_filter(const char *name);

G_MODULE_EXPORT void add_client_filter(const char *name, filter_function, void *userdata, int priority);
G_MODULE_EXPORT void del_client_filter(const char *name);

G_MODULE_EXPORT void add_server_filter(const char *name, filter_function, void *userdata, int priority);
G_MODULE_EXPORT void del_server_filter(const char *name);

typedef gboolean (*new_client_hook) (struct client *, void *userdata);
G_MODULE_EXPORT void add_new_client_hook(char *name, new_client_hook h, void *userdata);
G_MODULE_EXPORT void del_new_client_hook(char *name);

typedef void (*lose_client_hook) (struct client *, void *userdata);
G_MODULE_EXPORT void add_lose_client_hook(char *name, lose_client_hook h, void *userdata);
G_MODULE_EXPORT void del_lose_client_hook(char *name);

typedef char ** (*motd_hook) (struct network *n, void *userdata);
G_MODULE_EXPORT void add_motd_hook(char *name, motd_hook, void *userdata);
G_MODULE_EXPORT void del_motd_hook(char *name);

typedef void (*server_connected_hook) (struct network *, void *userdata);
G_MODULE_EXPORT void add_server_connected_hook(char *name, server_connected_hook h, void *userdata);
G_MODULE_EXPORT void del_server_connected_hook(char *name);

typedef void (*server_disconnected_hook) (struct network *, void *userdata);
G_MODULE_EXPORT void add_server_disconnected_hook(char *name, server_disconnected_hook h, void *userdata);
G_MODULE_EXPORT void del_server_disconnected_hook(char *name);

#if defined(_WIN32) && !defined(CTRLPROXY_CORE_BUILD)
G_MODULE_EXPORT gboolean fini_plugin(struct plugin *p);
G_MODULE_EXPORT gboolean init_plugin(struct plugin *p);
G_MODULE_EXPORT gboolean load_config(struct plugin *p, xmlNodePtr);
G_MODULE_EXPORT gboolean save_config(struct plugin *p, xmlNodePtr);
G_MODULE_EXPORT const char name_plugin[];
#pragma comment(lib,"ctrlproxy.lib")
#endif

G_MODULE_EXPORT void set_sslize_function (GIOChannel *(*) (GIOChannel *));

#endif /* __CTRLPROXY_H__ */
