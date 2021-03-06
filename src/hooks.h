/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2005 Jelmer Vernooĳ <jelmer@jelmer.uk>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __CTRLPROXY_HOOKS_H__
#define __CTRLPROXY_HOOKS_H__

/**
 * @file
 * @brief Hooks
 */

#include "connection.h"

enum data_direction { TO_SERVER = 1, FROM_SERVER = 2 };

/* Returns TRUE if filter should be continued, FALSE if it should be stopped. */
typedef gboolean (*server_filter_function) (struct irc_network *n, const struct irc_line *, enum data_direction, void *userdata);
G_MODULE_EXPORT void add_log_filter(const char *name, server_filter_function, void *userdata, int priority);
G_MODULE_EXPORT void del_log_filter(const char *name);

G_MODULE_EXPORT void add_server_filter(const char *name, server_filter_function, void *userdata, int priority);
G_MODULE_EXPORT void del_server_filter(const char *name);

typedef gboolean (*new_client_hook) (struct irc_client *, void *userdata);
G_MODULE_EXPORT void add_new_client_hook(const char *name, new_client_hook h, void *userdata);
G_MODULE_EXPORT void del_new_client_hook(const char *name);

typedef void (*lose_client_hook) (struct irc_client *, void *userdata);
G_MODULE_EXPORT void add_lose_client_hook(const char *name, lose_client_hook h, void *userdata);
G_MODULE_EXPORT void del_lose_client_hook(const char *name);

#endif /* __CTRLPROXY_HOOKS_H__ */
