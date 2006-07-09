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
#include "plugins.h"
#include "network.h"
#include "client.h"
#include "state.h"
#include "linestack.h"
#include "hooks.h"
#include "repl.h"
#include "ctcp.h"
#include "admin.h"

struct global {
	struct ctrlproxy_config *config;
	GList *new_network_notifiers;
	GList *networks;
	GList *nickserv_nicks;
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
G_MODULE_EXPORT int verify_client(const struct network *s, const struct client *c);
G_MODULE_EXPORT int str_rfc1459cmp(const char *a, const char *b);
G_MODULE_EXPORT int str_strictrfc1459cmp(const char *a, const char *b);
G_MODULE_EXPORT int str_asciicmp(const char *a, const char *b);

G_MODULE_EXPORT void set_sslize_function (GIOChannel *(*) (GIOChannel *, gboolean));
G_MODULE_EXPORT GIOChannel *sslize (GIOChannel *orig, gboolean server);

/* log.c */
enum log_level { LOG_DATA=5, LOG_TRACE=4, LOG_INFO=3, LOG_WARNING=2, LOG_ERROR=1 };
G_MODULE_EXPORT void log_network(const char *module, enum log_level, const struct network *, const char *fmt, ...);
G_MODULE_EXPORT void log_client(const char *module, enum log_level, const struct client *, const char *fmt, ...);
G_MODULE_EXPORT void log_global(const char *module, enum log_level, const char *fmt, ...);
G_MODULE_EXPORT void log_network_state(const char *module, enum log_level l, const struct network_state *st, const char *fmt, ...);

#endif /* __CTRLPROXY_H__ */
