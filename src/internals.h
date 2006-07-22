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
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
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
void fini_networks(struct global *);
void kill_pending_clients(const char *reason);

/* state.c */
void free_channels(struct network *s);
void network_nick_set_data(struct network_nick *n, const char *nick, const char *username, const char *host);

/* plugins.c */
gboolean init_plugins(const char *dir);

/* motd.c */
char **get_motd_lines(struct client *);

/* hooks.c */
void server_disconnected_hook_execute(struct network *);
void server_connected_hook_execute(struct network *);
gboolean new_client_hook_execute(struct client *c);
void lose_client_hook_execute(struct client *c);
gboolean run_client_filter(struct client *c, const struct line *l, enum data_direction dir);
gboolean run_server_filter(struct network *s, const struct line *l, enum data_direction dir);
gboolean run_log_filter(struct network *s, const struct line *l, enum data_direction dir);
gboolean run_replication_filter(struct network *s, const struct line *l, enum data_direction dir);

/* log.c */
gboolean init_log(const char *file);
void log_network_line(const struct network *n, const struct line *l, gboolean incoming);
void log_client_line(const struct client *c, const struct line *l, gboolean incoming);

/* redirect.c */
void redirect_record(const struct network *n, const struct client *c, const struct line *l);
void redirect_response(struct network *n, struct line *l);
void redirect_clear(const struct network *n);

/* cache.c */
gboolean client_try_cache(struct client *c, struct line *l);

/* linestack.c */
void init_linestack(struct ctrlproxy_config *);

/* gen_config.c */
void network_update_config(struct network_state *ns, struct network_config *nc);
void channel_update_config(struct channel_state *ns, struct channel_config *nc);
void global_update_config(struct global *my_global);

/* repl.c */
void client_replicate(struct client *);
char *mode2string(char modes[255]);
void string2mode(char *modes, char ar[255]);

/* main.c */
void free_global(struct global *);
void config_load_notify(struct global *global);
void config_save_notify(struct global *global, const char *);
struct global *new_global(const char *config_dir);

/* nickserv.c */
void init_nickserv(void);
gboolean nickserv_load(struct global *global);
gboolean nickserv_save(struct global *global, const char *dir);
void nickserv_identify_me(struct network *network, char *nick);

/* admin.c */
void init_admin(void);
gboolean admin_process_command(struct client *c, struct line *l, int cmdoffset);
void admin_log(const char *module, enum log_level level, const struct network *n, const struct client *c, const char *data);

/* settings.c */
gboolean create_configuration(const char *config_dir);

#endif /* __INTERNALS_H__ */
