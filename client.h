/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2005 Jelmer Vernooij <jelmer@nl.linux.org>

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

#ifndef __CTRLPROXY_CLIENT_H__
#define __CTRLPROXY_CLIENT_H__

#include "line.h"

#include <sys/time.h>
#include <glib.h>
#include <gmodule.h>

struct client {
	struct network *network;
	char *description;
	GIOChannel *incoming;
	gint incoming_id;
	gint ping_id;
	time_t last_ping;
	time_t last_pong;
	time_t connect_time;
	char *nick;
	char *fullname;
	char *hostname;
	char *username;
	int exit_on_close:1;
};

/* client.c */
G_MODULE_EXPORT struct client *new_client(struct network *, GIOChannel *, const char *desc);
G_MODULE_EXPORT void disconnect_client(struct client *c, const char *reason);
G_MODULE_EXPORT gboolean client_send_args(struct client *c, ...);
G_MODULE_EXPORT gboolean client_send_args_ex(struct client *c, const char *hm, ...);
G_MODULE_EXPORT gboolean client_send_response(struct client *c, int response, ...);
G_MODULE_EXPORT gboolean client_send_line(struct client *c, const struct line *);

#endif /* __CTRLPROXY_CLIENT_H__ */
