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

#include <time.h>
#include <stdarg.h>
#include <glib.h>
#include <gmodule.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

struct network;
struct client;
struct line;
struct transport_context;

enum data_direction { UNKNOWN = 0, TO_SERVER = 1, FROM_SERVER = 2 };
enum has_colon { COLON_UNKNOWN = 0, WITH_COLON = 1, WITHOUT_COLON = 2 };

typedef void (*disconnect_handler) (struct transport_context *, void *data);
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
};

struct transport_context {
	struct transport_ops *functions;
	xmlNodePtr configuration;
	void *data;
	void *caller_data;
	disconnect_handler on_disconnect;
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
	uint refcount;
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
	char did_nick_change;
};

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
};

struct plugin {
	char *name;
	GModule *module;
	xmlNodePtr xmlConf;
	void *data;
};

typedef gboolean (*plugin_init_function) (struct plugin *);
typedef gboolean (*plugin_fini_function) (struct plugin *);

/* state.c */
struct channel *find_channel(struct network *st, char *name);
struct channel_nick *find_nick(struct channel *c, char *name);
struct linestack_context *linestack_new_by_network(struct network *);
GSList *gen_replication_network(struct network *s);
GSList *gen_replication_channel(struct channel *c, char *hostmask, char *nick);
int is_channelname(char *name, struct network *s);

/* server.c */
struct network *connect_network(xmlNodePtr);
int close_network(struct network *s);
extern GList *networks;
void clients_send(struct network *, struct line *, struct transport_context *exception);
void network_add_listen(struct network *, xmlNodePtr);
void disconnect_client(struct client *c);

/* line.c */
struct line *linedup(struct line *l);
struct line * irc_parse_line(char *data);
struct line * virc_parse_line(char *origin, va_list ap);
char *irc_line_string(struct line *l);
char *irc_line_string_nl(struct line *l);
char *line_get_nick(struct line *l);
void free_line(struct line *l);
gboolean irc_send_args(struct transport_context *, ...);
gboolean irc_sendf(struct transport_context *, char *fmt, ...);
int irc_send_line(struct transport_context *, struct line *l);
struct line *irc_parse_linef( char *origin, ... );
struct line *irc_parse_line_args( char *origin, ... );

/* main.c */
extern GMainLoop *main_loop;
void clean_exit();
void save_configuration();
gboolean load_plugin(xmlNodePtr);
gboolean unload_plugin(struct plugin *);
gboolean plugin_loaded(char *name);

/* transport.c */
void register_transport(struct transport_ops *);
gboolean unregister_transport(char *name);
struct transport_context *transport_connect(const char *name, xmlNodePtr p, receive_handler, disconnect_handler, void *data);
struct transport_context *transport_listen(const char *name, xmlNodePtr p, newclient_handler, void *data);
void transport_free(struct transport_context *);
int transport_write(struct transport_context *, char *l);
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
	gboolean (*destroy) (struct linestack_context *);
};

struct linestack_context {
	struct linestack_ops *functions;
	void *data;
};

void register_linestack(struct linestack_ops *);
void unregister_linestack(struct linestack_ops *);
struct linestack_context *linestack_new(char *name, char *args);
GSList *linestack_get_linked_list(struct linestack_context *);
void linestack_send(struct linestack_context *, struct transport_context *);
gboolean linestack_destroy(struct linestack_context *);
gboolean linestack_clear(struct linestack_context *);
gboolean linestack_add_line(struct linestack_context *, struct line *);
gboolean linestack_add_line_list(struct linestack_context *, GSList *);

/* util.c */
char *list_make_string(char **);
xmlNodePtr xmlFindChildByName(xmlNodePtr parent, const xmlChar *name);
xmlNodePtr xmlFindChildByElementName(xmlNodePtr parent, const xmlChar *name);
extern xmlNodePtr xmlNode_networks, xmlNode_plugins;
extern GList *plugins;
extern xmlDocPtr configuration;
int verify_client(struct network *s, struct client *c);
char *ctrlproxy_path(char *part);

/* hooks.c */
/* Returns TRUE if filter should be continued, FALSE if it should be stopped. */
typedef gboolean (*filter_function) (struct line *);
void add_filter(char *name, filter_function);
gboolean add_filter_ex(char *name, filter_function, char *classname, int priority);
void del_filter(filter_function);
gboolean del_filter_ex(char *classname, filter_function);
gboolean filters_execute(struct line *l);
void add_filter_class(char *name, int priority);

typedef gboolean (*new_client_hook) (struct client *);
void add_new_client_hook(char *name, new_client_hook h);
void del_new_client_hook(char *name);
gboolean new_client_hook_execute(struct client *c);

typedef void (*lose_client_hook) (struct client *);
void add_lose_client_hook(char *name, lose_client_hook h);
void del_lose_client_hook(char *name);
void lose_client_hook_execute(struct client *c);

typedef char ** (*motd_hook) (struct network *n);
void add_motd_hook(char *name, motd_hook);
void del_motd_hook(char *name);
char **get_motd_lines(struct network *n);

typedef void (*server_connected_hook) (struct network *);
void add_server_connected_hook(char *name, server_connected_hook h);
void del_server_connected_hook(char *name);
void server_connected_hook_execute(struct network *);

typedef void (*server_disconnected_hook) (struct network *);
void add_server_disconnected_hook(char *name, server_disconnected_hook h);
void del_server_disconnected_hook(char *name);
void server_disconnected_hook_execute(struct network *);

typedef void (*initialized_hook) (void);
void add_initialized_hook(initialized_hook);
void initialized_hook_execute(void);

/* log.c */

#endif /* __CTRLPROXY_H__ */
