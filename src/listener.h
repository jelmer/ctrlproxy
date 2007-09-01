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

G_MODULE_EXPORT struct listener *listener_init(struct global *global, struct listener_config *);
G_MODULE_EXPORT gboolean start_listener(struct listener *);
G_MODULE_EXPORT gboolean stop_listener(struct listener *);
G_MODULE_EXPORT void fini_listeners(struct global *);
G_MODULE_EXPORT void free_listener(struct listener *l);
G_MODULE_EXPORT gboolean init_listeners(struct global *global);

#endif
