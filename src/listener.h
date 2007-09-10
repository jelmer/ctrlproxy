#ifndef __CTRLPROXY_LISTENER_H__
#define __CTRLPROXY_LISTENER_H__

#include <netdb.h>
#include "ctrlproxy.h"

#ifndef G_MODULE_EXPORT
#define G_MODULE_EXPORT
#endif


/**
 * A listener.
 */
struct listener {
	int active:1;
	GList *incoming;
	GList *pending;
	struct listener_config *config;
	struct network *network;
	struct global *global;
};

struct listener_iochannel {
	char address[NI_MAXHOST];
	char port[NI_MAXSERV];
	gint watch_id;
};

struct socks_method;

struct pending_client {
	GIOChannel *connection;
	const char *user;
	const char *password;
	gint watch_id;
	struct sockaddr *clientname;
	socklen_t clientname_len;
	struct listener *listener;
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

G_MODULE_EXPORT struct listener *listener_init(struct global *global, struct listener_config *);
G_MODULE_EXPORT gboolean start_listener(struct listener *);
G_MODULE_EXPORT gboolean stop_listener(struct listener *);
G_MODULE_EXPORT void fini_listeners(struct global *);
G_MODULE_EXPORT void free_listener(struct listener *l);
G_MODULE_EXPORT gboolean init_listeners(struct global *global);

#endif
