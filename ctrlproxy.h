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
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#ifdef STRICT_MEMORY_ALLOCS
#define malloc(a) __ERROR_USE_G_MALLOC_OR_G_NEW__
#define realloc(a,b) __ERROR_USE_G_REALLOC_OR_G_RE_NEW__
#define free(a) __ERROR_USE_G_FREE__
#define strdup(a) __ERROR_USE_G_STRDUP__
#define strndup(a) __ERROR_USE_G_STRNDUP__
#endif

struct network;
struct client;
struct line;
struct channel_nick;
struct network_nick;
struct transport_context;

enum data_direction { UNKNOWN = 0, TO_SERVER = 1, FROM_SERVER = 2 };
enum has_colon { COLON_UNKNOWN = 0, WITH_COLON = 1, WITHOUT_COLON = 2 };

typedef void (*disconnect_handler) (struct transport_context *, void *data);
typedef void (*connected_handler) (struct transport_context *, void *data);
typedef void (*receive_handler) (struct transport_context *, char *l, void *data);
typedef void (*newclient_handler) (struct transport_context *, struct transport_context *, void *data);

struct transport_ops {
	char *name;
	/* The connect and listen functions should add something to the poll */
	int (*connect) (struct transport_context *context);
	int (*listen) (struct transport_context *context);
	int (*write) (struct transport_context *context, char *l);
	int (*close) (struct transport_context *context);
	int reference_count;
	struct plugin *plugin;
};

struct transport_context {
	struct transport_ops *functions;
	xmlNodePtr configuration;
	void *data;
	void *caller_data;
	disconnect_handler on_disconnect;
	connected_handler on_connect;
	receive_handler on_receive;
	newclient_handler on_new_client;
};

struct line {
	enum data_direction direction;
	int options;
	struct network *network;
	struct client *client;
	const char *origin;
	char **args; /* NULL terminated */
	size_t argc;
	/* Don't use the following properties. Use line_get_network_nick() 
	 * instead. */
	struct network_nick *network_nick; 
	enum has_colon has_endcolon;
};

/* for the options fields */
#define LINE_IS_PRIVATE      1
#define LINE_DONT_SEND       2
#define LINE_STOP_PROCESSING 4
#define LINE_NO_LOGGING      8

struct channel_nick {
	char mode;
	struct channel *channel;
	struct network_nick *global;
};

struct network_nick {
	guint refcount;
	char *name;
	char *hostmask;
	GList *channels;
};

struct channel {
	xmlNodePtr xmlConf;
	char *topic;
	char mode; /* Private, secret, etc */
	char modes[255];
	char introduced;
	char namreply_started;
	long limit;
	GList *nicks;
	struct network *network;
};

struct client {
	struct network *network;
	char authenticated;
	struct transport_context *incoming;
	time_t connect_time;
	char* nick;
};

enum casemapping { CASEMAP_UNKNOWN = 0, CASEMAP_RFC1459 = 1, CASEMAP_ASCII = 2 };

struct network {
	xmlNodePtr xmlConf;
	char mymodes[255];
	xmlNodePtr servers;
	char *hostmask;
	GList *channels;
	GList *nicks;
	char authenticated;
	GList *clients;
	xmlNodePtr current_server;
	xmlNodePtr listen;
	char *supported_modes[2];
	char **features;
	struct transport_context *outgoing;
	struct transport_context **incoming;
	guint reconnect_id;
	enum casemapping casemapping;
	gboolean name_guessed;
};

struct plugin {
	char *name;
	char *path;
	GModule *module;
	xmlNodePtr xmlConf;
	void *data;
};

typedef gboolean (*plugin_init_function) (struct plugin *);
typedef gboolean (*plugin_fini_function) (struct plugin *);


/* state.c */
G_MODULE_EXPORT struct channel *find_channel(struct network *st, char *name);
G_MODULE_EXPORT struct channel_nick *find_nick(struct channel *c, char *name);
G_MODULE_EXPORT struct linestack_context *linestack_new_by_network(struct network *);
G_MODULE_EXPORT GSList *gen_replication_network(struct network *s);
G_MODULE_EXPORT GSList *gen_replication_channel(struct channel *c, char *hostmask, char *nick);
G_MODULE_EXPORT int is_channelname(char *name, struct network *s);
G_MODULE_EXPORT int is_prefix(char p, struct network *n);
G_MODULE_EXPORT char get_prefix_by_mode(char p, struct network *n);
G_MODULE_EXPORT const char *get_network_feature(struct network *n, char *name);
G_MODULE_EXPORT int irccmp(struct network *n, const char *a, const char *b);
G_MODULE_EXPORT struct network_nick *line_get_network_nick(struct line *l);

/* server.c */
G_MODULE_EXPORT struct network *connect_network(xmlNodePtr);
G_MODULE_EXPORT gboolean connect_current_server (struct network *);
G_MODULE_EXPORT gboolean connect_next_server (struct network *);
G_MODULE_EXPORT int close_network(struct network *s);
G_MODULE_EXPORT gboolean close_server(struct network *n);
G_MODULE_EXPORT GList *get_network_list();
G_MODULE_EXPORT void clients_send(struct network *, struct line *, struct transport_context *exception);
G_MODULE_EXPORT void network_add_listen(struct network *, xmlNodePtr);
G_MODULE_EXPORT void disconnect_client(struct client *c);
G_MODULE_EXPORT void server_send_login (struct transport_context *c, void *_server);
G_MODULE_EXPORT gboolean network_change_nick(struct network *s, char *nick);

/* line.c */
G_MODULE_EXPORT struct line *linedup(struct line *l);
G_MODULE_EXPORT struct line * irc_parse_line(char *data);
G_MODULE_EXPORT struct line * virc_parse_line(char *origin, va_list ap);
G_MODULE_EXPORT char *irc_line_string(struct line *l);
G_MODULE_EXPORT char *irc_line_string_nl(struct line *l);
G_MODULE_EXPORT char *line_get_nick(struct line *l);
G_MODULE_EXPORT void free_line(struct line *l);
G_MODULE_EXPORT gboolean irc_send_args(struct transport_context *, ...);
G_MODULE_EXPORT gboolean irc_sendf(struct transport_context *, char *fmt, ...);
G_MODULE_EXPORT int irc_send_line(struct transport_context *, struct line *l);
G_MODULE_EXPORT struct line *irc_parse_linef( char *origin, ... );
G_MODULE_EXPORT struct line *irc_parse_line_args( char *origin, ... );

/* main.c */
G_MODULE_EXPORT void clean_exit();
G_MODULE_EXPORT const char *ctrlproxy_version();
G_MODULE_EXPORT const char *get_my_hostname();
G_MODULE_EXPORT const char *get_modules_path();
G_MODULE_EXPORT const char *get_shared_path();

/* config.c */
G_MODULE_EXPORT void save_configuration();
G_MODULE_EXPORT xmlNodePtr config_node_root();
G_MODULE_EXPORT xmlNodePtr config_node_networks();
G_MODULE_EXPORT xmlNodePtr config_node_plugins();
G_MODULE_EXPORT xmlDocPtr config_doc();

/* plugins.c */
G_MODULE_EXPORT gboolean load_plugin(xmlNodePtr);
G_MODULE_EXPORT gboolean unload_plugin(struct plugin *);
G_MODULE_EXPORT gboolean plugin_loaded(char *name);
G_MODULE_EXPORT void push_plugin(struct plugin *p);
G_MODULE_EXPORT struct plugin *peek_plugin();
G_MODULE_EXPORT struct plugin *pop_plugin();
G_MODULE_EXPORT GList *get_plugin_list();

/* transport.c */
G_MODULE_EXPORT GList *get_transport_list();
G_MODULE_EXPORT void register_transport(struct transport_ops *);
G_MODULE_EXPORT gboolean unregister_transport(char *name);
struct transport_context *transport_connect(const char *name, xmlNodePtr p, receive_handler, connected_handler, disconnect_handler, void *data);
struct transport_context *transport_listen(const char *name, xmlNodePtr p, newclient_handler, void *data);
void transport_free(struct transport_context *);
G_MODULE_EXPORT int transport_write(struct transport_context *, char *l);
void transport_set_disconnect_handler(struct transport_context *, disconnect_handler);
void transport_set_receive_handler(struct transport_context *, receive_handler);
void transport_set_newclient_handler(struct transport_context *, newclient_handler);
void transport_set_data(struct transport_context *, void *);

/* linestack.c */
struct linestack_context;
struct linestack_ops {
	char *name;
	gboolean (*init) (struct linestack_context *, char *args);
	gboolean (*clear) (struct linestack_context *);
	gboolean (*add_line) (struct linestack_context *, struct line *);
	GSList *(*get_linked_list) (struct linestack_context *);
	void (*send) (struct linestack_context *, struct transport_context *);
	void (*send_limited) (struct linestack_context *, struct transport_context *, size_t);
	gboolean (*destroy) (struct linestack_context *);
};

struct linestack_context {
	struct linestack_ops *functions;
	void *data;
};

G_MODULE_EXPORT void register_linestack(struct linestack_ops *);
G_MODULE_EXPORT void unregister_linestack(struct linestack_ops *);
G_MODULE_EXPORT struct linestack_context *linestack_new(char *name, char *args);
G_MODULE_EXPORT GSList *linestack_get_linked_list(struct linestack_context *);
G_MODULE_EXPORT void linestack_send(struct linestack_context *, struct transport_context *);
G_MODULE_EXPORT gboolean linestack_destroy(struct linestack_context *);
G_MODULE_EXPORT gboolean linestack_clear(struct linestack_context *);
G_MODULE_EXPORT gboolean linestack_add_line(struct linestack_context *, struct line *);
G_MODULE_EXPORT gboolean linestack_add_line_list(struct linestack_context *, GSList *);

/* util.c */
G_MODULE_EXPORT char *list_make_string(char **);
G_MODULE_EXPORT xmlNodePtr xmlFindChildByName(xmlNodePtr parent, const xmlChar *name);
G_MODULE_EXPORT xmlNodePtr xmlFindChildByElementName(xmlNodePtr parent, const xmlChar *name);
G_MODULE_EXPORT int verify_client(struct network *s, struct client *c);
G_MODULE_EXPORT char *ctrlproxy_path(char *part);
G_MODULE_EXPORT int strrfc1459cmp(const char *a, const char *b);

/* hooks.c */
/* Returns TRUE if filter should be continued, FALSE if it should be stopped. */
typedef gboolean (*filter_function) (struct line *);
G_MODULE_EXPORT void add_filter(char *name, filter_function);
G_MODULE_EXPORT gboolean add_filter_ex(char *name, filter_function, char *classname, int priority);
G_MODULE_EXPORT void del_filter(filter_function);
G_MODULE_EXPORT gboolean del_filter_ex(char *classname, filter_function);
G_MODULE_EXPORT gboolean filters_execute(struct line *l);
G_MODULE_EXPORT gboolean filters_execute_class(char *name, struct line *l);
G_MODULE_EXPORT void add_filter_class(char *name, int priority);

typedef gboolean (*new_client_hook) (struct client *);
G_MODULE_EXPORT void add_new_client_hook(char *name, new_client_hook h);
G_MODULE_EXPORT void del_new_client_hook(char *name);
G_MODULE_EXPORT gboolean new_client_hook_execute(struct client *c);

typedef void (*lose_client_hook) (struct client *);
G_MODULE_EXPORT void add_lose_client_hook(char *name, lose_client_hook h);
G_MODULE_EXPORT void del_lose_client_hook(char *name);
G_MODULE_EXPORT void lose_client_hook_execute(struct client *c);

typedef char ** (*motd_hook) (struct network *n);
G_MODULE_EXPORT void add_motd_hook(char *name, motd_hook);
G_MODULE_EXPORT void del_motd_hook(char *name);
G_MODULE_EXPORT char **get_motd_lines(struct network *n);

typedef void (*server_connected_hook) (struct network *);
G_MODULE_EXPORT void add_server_connected_hook(char *name, server_connected_hook h);
G_MODULE_EXPORT void del_server_connected_hook(char *name);
G_MODULE_EXPORT void server_connected_hook_execute(struct network *);

typedef void (*server_disconnected_hook) (struct network *);
G_MODULE_EXPORT void add_server_disconnected_hook(char *name, server_disconnected_hook h);
G_MODULE_EXPORT void del_server_disconnected_hook(char *name);
G_MODULE_EXPORT void server_disconnected_hook_execute(struct network *);

typedef void (*initialized_hook) (void);
G_MODULE_EXPORT void add_initialized_hook(initialized_hook);
G_MODULE_EXPORT void initialized_hook_execute(void);

#ifdef _WIN32
G_MODULE_EXPORT int asprintf(char **dest, const char *fmt, ...);
G_MODULE_EXPORT int vasprintf(char **dest, const char *fmt, va_list ap);
#endif

#if defined(_WIN32) && !defined(CTRLPROXY_CORE_BUILD)
G_MODULE_EXPORT gboolean fini_plugin(struct plugin *p);
G_MODULE_EXPORT gboolean init_plugin(struct plugin *p);
G_MODULE_EXPORT const char name_plugin[];
#pragma comment(lib,"ctrlproxy.lib")
#endif

#endif /* __CTRLPROXY_H__ */
