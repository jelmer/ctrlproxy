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

#ifndef __INTERNALS_H__
#define __INTERNALS_H__

#define CTRLPROXY_CORE_BUILD

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif
#ifdef HAVE_POPT_H
#  include <popt.h>
#endif
#include "ctrlproxy.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>

#undef  G_LOG_DOMAIN
#define G_LOG_DOMAIN "ctrlproxy"

#define DEFAULT_RECONNECT_INTERVAL 60

#define MAXHOSTNAMELEN 4096

/* server.c */
int loop(struct network *server); /* Checks server socket for input and calls loop() on all of it's modules */
gboolean init_networks();
void kill_pending_clients(void);

/* state.c */
void state_handle_data(struct network *s, struct line *l);
void free_channels(struct network *s);

/* config.c */
void init_config(void);
void fini_config(void);

/* plugins.c */
gboolean init_plugins(void);
void fini_plugins(void);

/* hooks.c */
void server_disconnected_hook_execute(struct network *);
void server_connected_hook_execute(struct network *);
char **get_motd_lines(struct network *n);
gboolean new_client_hook_execute(struct client *c);
void lose_client_hook_execute(struct client *c);
gboolean run_client_filter(struct line *l);
gboolean run_server_filter(struct line *l);
gboolean run_log_filter(struct line *l);
gboolean run_replication_filter(struct line *l);

#endif /* __INTERNALS_H__ */
