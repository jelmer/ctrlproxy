%module ctrlproxy
%{
#include "ctrlproxy.h"
%}
#if 0
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
	char *origin;
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
	struct network_nick *global_nick;
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
	char *description;
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
struct channel *find_channel(struct network *st, char *name);
struct channel_nick *find_nick(struct channel *c, char *name);
struct linestack_context *linestack_new_by_network(struct network *);
GSList *gen_replication_network(struct network *s);
GSList *gen_replication_channel(struct channel *c, char *hostmask, char *nick);
int is_channelname(char *name, struct network *s);
int is_prefix(char p, struct network *n);
char get_prefix_by_mode(char p, struct network *n);
const char *get_network_feature(struct network *n, char *name);
int irccmp(struct network *n, const char *a, const char *b);
struct network_nick *line_get_network_nick(struct line *l);
struct network *find_network_by_xml(xmlNodePtr cur);

/* server.c */
struct network *connect_network(xmlNodePtr);
gboolean connect_current_server (struct network *);
gboolean connect_next_server (struct network *);
int close_network(struct network *s);
gboolean close_server(struct network *n);
GList *get_network_list(void);
void clients_send(struct network *, struct line *, struct transport_context *exception);
void network_add_listen(struct network *, xmlNodePtr);
void disconnect_client(struct client *c);
void server_send_login (struct transport_context *c, void *_server);
gboolean network_change_nick(struct network *s, char *nick);

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
const char *ctrlproxy_version(void);
const char *get_my_hostname(void);
const char *get_modules_path(void);
const char *get_shared_path(void);

/* config.c */
void save_configuration(void);
xmlNodePtr config_node_root(void);
xmlNodePtr config_node_networks(void);
xmlNodePtr config_node_plugins(void);
xmlDocPtr config_doc(void);

/* plugins.c */
gboolean load_plugin(xmlNodePtr);
gboolean unload_plugin(struct plugin *);
gboolean plugin_loaded(char *name);
void push_plugin(struct plugin *p);
struct plugin *peek_plugin(void);
struct plugin *pop_plugin(void);
GList *get_plugin_list(void);

/* transport.c */
GList *get_transport_list(void);
void register_transport(struct transport_ops *);
gboolean unregister_transport(char *name);
struct transport_context *transport_connect(const char *name, xmlNodePtr p, receive_handler, connected_handler, disconnect_handler, void *data);
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
	void (*send_limited) (struct linestack_context *, struct transport_context *, size_t);
	gboolean (*destroy) (struct linestack_context *);
};
#endif

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
int verify_client(struct network *s, struct client *c);
char *ctrlproxy_path(char *part);
int strrfc1459cmp(const char *a, const char *b);

/* hooks.c */
/* Returns TRUE if filter should be continued, FALSE if it should be stopped. */
typedef gboolean (*filter_function) (struct line *);
void add_filter(char *name, filter_function);
gboolean add_filter_ex(char *name, filter_function, char *classname, int priority);
void del_filter(filter_function);
gboolean del_filter_ex(char *classname, filter_function);
gboolean filters_execute(struct line *l);
gboolean filters_execute_class(char *name, struct line *l);
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
