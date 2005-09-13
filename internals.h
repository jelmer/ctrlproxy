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

#ifndef __INTERNALS_H__
#define __INTERNALS_H__

#define CTRLPROXY_CORE_BUILD

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif
#ifdef HAVE_POPT_H
#  include <popt.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#define __USE_POSIX
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "ctrlproxy.h"

#define DEFAULT_RECONNECT_INTERVAL 60
#define MAXHOSTNAMELEN 4096

/* server.c */
gboolean init_networks();
void fini_networks(void);
void kill_pending_clients(const char *reason);

/* state.c */
void state_handle_data(struct network_state *s, struct line *l);
void free_channels(struct network *s);
void network_nick_set_data(struct network_nick *n, const char *nick, const char *username, const char *host);
void network_nick_set_hostmask(struct network_nick *n, const char *hm);
void log_network_state(struct network_state *st, const char *, ...);

/* config.c */
void init_config(void);
void fini_config(void);

/* plugins.c */
gboolean init_plugins(struct ctrlproxy_config *);
void fini_plugins(void);

/* hooks.c */
void server_disconnected_hook_execute(struct network *);
void server_connected_hook_execute(struct network *);
char **get_motd_lines(struct network *n);
gboolean new_client_hook_execute(struct client *c);
void lose_client_hook_execute(struct client *c);
gboolean run_client_filter(struct client *c, struct line *l, enum data_direction dir);
gboolean run_server_filter(struct network *s, struct line *l, enum data_direction dir);
gboolean run_log_filter(struct network *s, struct line *l, enum data_direction dir);
gboolean run_replication_filter(struct network *s, struct line *l, enum data_direction dir);

/* log.c */
gboolean init_log(const char *file);
void fini_log(void);

/* redirect.c */
void redirect_record(struct client *c, struct line *l);
void redirect_response(struct network *n, struct line *l);

/* cache.c */
gboolean client_try_cache(struct client *c, struct line *l);

/* linestack.c */
void init_linestack(struct ctrlproxy_config *);
void fini_linestack(void);
gboolean linestack_insert_line(const struct network *n, const struct line *l, enum data_direction dir);

/* gen_config.c */
void network_update_config(struct network_state *ns, struct network_config *nc);
void channel_update_config(struct channel_state *ns, struct channel_config *nc);
void plugin_update_config(struct plugin *ps, struct plugin_config *pc);

#endif /* __INTERNALS_H__ */
