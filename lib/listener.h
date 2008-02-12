#ifndef __CTRLPROXY_LISTENER_H__
#define __CTRLPROXY_LISTENER_H__

#include <netdb.h>
#include "ctrlproxy.h"

#ifndef G_MODULE_EXPORT
#define G_MODULE_EXPORT
#endif

struct irc_listener;

typedef void (*listener_log_fn) (enum log_level, const struct irc_listener *, const char *);

struct pending_client;

/**
 * A listener.
 */
struct irc_listener {
	int active:1;
	GIConv iconv;
	GList *incoming;
	GList *pending;
	struct listener_config *config;
	struct irc_network *network;
	struct global *global;
	listener_log_fn log_fn;
	gboolean (*handle_client_line) (struct pending_client *pc, const struct irc_line *l);
	void (*client_accepted_fn)(struct pending_client *);
};


struct socks_method;

/**
 * Client connection that has not been authenticated yet.
 */
struct pending_client {
	GIOChannel *connection;
	char *user;
	char *password;
	gint watch_id;
	struct sockaddr *clientname;
	socklen_t clientname_len;
	struct irc_listener *listener;
	struct {
		struct socks_method *method;
		enum state { 
			SOCKS_UNUSED = -1,
			SOCKS_UNKNOWN = 0,
			SOCKS_STATE_NEW = 1, 
			SOCKS_STATE_AUTH, 
			SOCKS_STATE_NORMAL 
		} state;
		void *method_data;
	} socks;
};

G_MODULE_EXPORT gboolean listener_start(struct irc_listener *, const char *address, const char *service);
G_MODULE_EXPORT gboolean listener_stop(struct irc_listener *);
G_MODULE_EXPORT void fini_listeners(struct global *);
G_MODULE_EXPORT void free_listener(struct irc_listener *l);
G_MODULE_EXPORT gboolean init_listeners(struct global *global);
G_MODULE_EXPORT void listener_log(enum log_level l, const struct irc_listener *listener,
				 const char *fmt, ...);

#endif
