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

#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <glib.h>
#include <gmodule.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <sys/select.h>

struct network;
struct client;
struct line;
struct transport_context;

enum data_direction { UNKNOWN = 0, TO_SERVER = 1, FROM_SERVER = 2 };

typedef void (*disconnect_handler) (struct transport_context *, void *data);
typedef void (*receive_handler) (struct transport_context *, char *l, void *data);
typedef void (*newclient_handler) (struct transport_context *, struct transport_context *, void *data);

struct transport {
	char *name;
	/* The connect and listen functions should add something to the poll */
	int (*connect) (struct transport_context *context);
	int (*listen) (struct transport_context *context);
	int (*write) (struct transport_context *context, char *l);
	int (*close) (struct transport_context *context);
	int reference_count;
};

struct transport_context {
	struct transport *functions;
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
};

/* for the options fields */
#define LINE_IS_PRIVATE      1 
#define LINE_DONT_SEND       2 
#define LINE_STOP_PROCESSING 4

struct nick {
	char *name;
	char mode;
};

struct channel {
	xmlNodePtr xmlConf;
	char *topic;
	char mode; /* Private, secret, etc */
	char *modes[255];
	char introduced;
	char namreply_started;
	long limit;
	GList *nicks;
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
	char modes[255];
	xmlNodePtr servers;
	char *hostmask;
	GList *channels;
	char authenticated;
	GList *clients;
	xmlNodePtr current_server;
	xmlNodePtr listen;
	void  *replication_data;
	char *supported_modes[2];
	char **features;
	struct transport_context *outgoing;
	struct transport_context **incoming;
};

struct plugin {
	char *name;
	GModule *module;		
	xmlNodePtr xmlConf;
	void *data;
};

typedef gboolean (*plugin_init_function) (struct plugin *);
typedef gboolean (*plugin_fini_function) (struct plugin *);
extern void (*replicate_function) (struct network *, struct transport_context *);

/* state.c */
struct channel *find_channel(struct network *st, char *name);
struct nick *find_nick(struct channel *c, char *name);
GSList *gen_replication(struct network *s);

/* server.c */
struct network *connect_to_server(xmlNodePtr);
int close_server(struct network *s);
void default_replicate_function (struct network *, struct transport_context *);
extern GList *networks;
void clients_send(struct network *, struct line *, struct transport_context *exception);
void network_add_listen(struct network *, xmlNodePtr);

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
void clean_exit();
void save_configuration();
gboolean load_plugin(xmlNodePtr);
gboolean unload_plugin(struct plugin *);
gboolean plugin_loaded(char *name);

/* transport.c */
void register_transport(struct transport *);
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
struct linestack {
	char *name;
	gboolean (*init) (struct linestack_context *, char *args);
	gboolean (*clear) (struct linestack_context *);
	gboolean (*add_line) (struct linestack_context *, struct line *);
	GSList *(*get_linked_list) (struct linestack_context *);
	void (*send) (struct linestack_context *, struct transport_context *);
	gboolean (*destroy) (struct linestack_context *);
};

struct linestack_context {
	struct linestack *functions;
	void *data;
};

void register_linestack(struct linestack *);
void unregister_linestack(struct linestack *);
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

/* hooks.c */
/* Returns TRUE if filter should be continued, FALSE if it should be stopped. */
typedef gboolean (*filter_function) (struct line *);
void add_filter(char *name, filter_function);
void del_filter(filter_function);

#endif /* __CTRLPROXY_H__ */
