#ifndef __CTRLPROXY_LISTENER_H__
#define __CTRLPROXY_LISTENER_H__

#include "ctrlproxy.h"

#ifndef G_MODULE_EXPORT
#define G_MODULE_EXPORT
#endif

struct listener {
	guint16 port;
	GIOChannel *incoming;
	gint incoming_id;
	GList *pending;
	char *password;
	struct network *network;
};

G_MODULE_EXPORT struct listener *new_listener(guint16 port);
G_MODULE_EXPORT gboolean start_listener(struct listener *);
G_MODULE_EXPORT gboolean stop_listener(struct listener *);

#if defined(_WIN32) && !defined(LISTENER_CORE_BUILD)
#pragma comment(lib,"liblistener.lib")
#endif

#endif