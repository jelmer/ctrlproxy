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

#define CTRLPROXY_PLUGIN_VERSION 1

/**
 * @file
 * @brief Main functions
 */

#if defined(STRICT_MEMORY_ALLOCS) && !defined(SWIGPYTHON)
#define calloc(a,b) __ERROR_USE_G_NEW0__
#define malloc(a) __ERROR_USE_G_MALLOC_OR_G_NEW__
#define realloc(a,b) __ERROR_USE_G_REALLOC_OR_G_RE_NEW__
#define free(a) __ERROR_USE_G_FREE__
#undef strdup
#define strdup(a) __ERROR_USE_G_STRDUP__
#undef strndup
#define strndup(a) __ERROR_USE_G_STRNDUP__
#endif

#ifndef G_MODULE_EXPORT
#define G_MODULE_EXPORT 
#endif

#include "settings.h"
#include "network.h"
#include "client.h"
#include "state.h"
#include "linestack.h"
#include "hooks.h"
#include "repl.h"
#include "ctcp.h"
#include "admin.h"
#include "log.h"
#include "isupport.h"
#include "log_support.h"
#include "listener.h"

/**
 * Global information.
 */
struct global {
	struct ctrlproxy_config *config;
	GList *new_network_notifiers;
	GList *networks;
	GList *nickserv_nicks;
	GList *listeners;

	GIOChannel *unix_incoming;
	gint unix_incoming_id;

	GIOChannel *admin_incoming;
	gint admin_incoming_id;
};

struct plugin_ops {
	int version;
	char *name;
	gboolean (*init) (void);
};

/* main.c */
G_MODULE_EXPORT const char *ctrlproxy_version(void);
G_MODULE_EXPORT const char *get_my_hostname(void);

typedef void (*config_load_notify_fn) (struct global *);
typedef void (*config_save_notify_fn) (struct global *, const char *);
G_MODULE_EXPORT void register_load_config_notify(config_load_notify_fn fn);
G_MODULE_EXPORT void register_save_config_notify(config_save_notify_fn fn);

/* util.c */
G_MODULE_EXPORT char *list_make_string(GList *);
G_MODULE_EXPORT int verify_client(const struct network *s, 
								  const struct client *c);
G_MODULE_EXPORT int str_rfc1459cmp(const char *a, const char *b);
G_MODULE_EXPORT int str_strictrfc1459cmp(const char *a, const char *b);
G_MODULE_EXPORT int str_asciicmp(const char *a, const char *b);
G_MODULE_EXPORT char *g_io_channel_ip_get_description(GIOChannel *ch);

/* log.c */
G_GNUC_PRINTF(3, 4) G_MODULE_EXPORT void log_network_state(enum log_level l, const struct network_state *st, const char *fmt, ...);

gboolean    rep_g_file_get_contents             (const gchar *filename,
                                             gchar **contents,
                                             gsize *length,
                                             GError **error);
gboolean    rep_g_file_set_contents             (const gchar *filename,
                                             const gchar *contents,
                                             gssize length,
                                             GError **error);
#if GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION < 8
#define g_file_get_contents rep_g_file_get_contents
#define g_file_set_contents rep_g_file_set_contents
#endif

#endif /* __CTRLPROXY_H__ */
