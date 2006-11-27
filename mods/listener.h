#ifndef __CTRLPROXY_LISTENER_H__
#define __CTRLPROXY_LISTENER_H__

#include "ctrlproxy.h"

#ifndef G_MODULE_EXPORT
#define G_MODULE_EXPORT
#endif

struct listener {
	int active:1;
	int ssl:1;
	GIOChannel *incoming;
	gint incoming_id;
	GList *pending;
	char *password;
	char *address;
	char *port;
	struct network *network;
	gpointer ssl_credentials;
};

G_MODULE_EXPORT struct listener *listener_init(const char *addr, const char *port);
G_MODULE_EXPORT gboolean start_listener(struct listener *);
G_MODULE_EXPORT gboolean stop_listener(struct listener *);

#if defined(_WIN32) && !defined(LISTENER_CORE_BUILD)
#pragma comment(lib,"liblistener.lib")
#endif

#endif
