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

#include <libxml/tree.h>

#ifdef STRICT_MEMORY_ALLOCS
#define calloc(a,b) __ERROR_USE_G_NEW0__
#define malloc(a) __ERROR_USE_G_MALLOC_OR_G_NEW__
#define realloc(a,b) __ERROR_USE_G_REALLOC_OR_G_RE_NEW__
#define free(a) __ERROR_USE_G_FREE__
#define strdup(a) __ERROR_USE_G_STRDUP__
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

/* main.c */
G_MODULE_EXPORT const char *ctrlproxy_version(void);
G_MODULE_EXPORT const char *get_my_hostname(void);
G_MODULE_EXPORT struct ctrlproxy_config *get_current_config(void);

/* util.c */
G_MODULE_EXPORT char *list_make_string(GList *);
G_MODULE_EXPORT int verify_client(const struct network *s, const struct client *c);
G_MODULE_EXPORT char *ctrlproxy_path(const char *part);
G_MODULE_EXPORT int str_rfc1459cmp(const char *a, const char *b);
G_MODULE_EXPORT int str_strictrfc1459cmp(const char *a, const char *b);
G_MODULE_EXPORT int str_asciicmp(const char *a, const char *b);

G_MODULE_EXPORT void set_sslize_function (GIOChannel *(*) (GIOChannel *, gboolean));
G_MODULE_EXPORT GIOChannel *sslize (GIOChannel *orig, gboolean server);

/* log.c */
enum log_level { LOG_DATA=5, LOG_TRACE=4, LOG_INFO=3, LOG_WARNING=2, LOG_ERROR=1 };
void log_network(const char *module, enum log_level, const struct network *, const char *fmt, ...);
void log_client(const char *module, enum log_level, const struct client *, const char *fmt, ...);
void log_global(const char *module, enum log_level, const char *fmt, ...);

#endif /* __CTRLPROXY_H__ */
