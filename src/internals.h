/* 
	ctrlproxy: A modular IRC proxy
	(c) 2002-2005 Jelmer Vernooij <jelmer@nl.linux.org>

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
#include "plugins.h"
#include "local.h"

#define DEFAULT_RECONNECT_INTERVAL 	60
#define MIN_SILENT_TIME				60
#define MAX_SILENT_TIME 			(2*MIN_SILENT_TIME)

/* server.c */
void fini_networks(struct global *);
void kill_pending_clients(const char *reason);
gboolean network_set_iochannel(struct irc_network *s, GIOChannel *ioc);

/* state.c */
void free_channels(struct irc_network *s);
void network_nick_set_data(struct network_nick *n, const char *nick, const char *username, const char *host);

/* plugins.c */
gboolean init_plugins(const char *dir);

/* motd.c */
char **get_motd_lines(const char *motd_file);

/* network.c */
G_MODULE_EXPORT struct irc_network *load_network(struct global *global, struct network_config *sc);
G_MODULE_EXPORT gboolean load_networks(struct global *, struct ctrlproxy_config *cfg, network_log_fn);
G_MODULE_EXPORT void unload_network(struct irc_network *);
typedef void (*new_network_notify_fn) (struct irc_network *, void *);
G_MODULE_EXPORT struct irc_network *find_network_by_hostname(struct global *global, const char *host, guint16 port, gboolean create);
G_MODULE_EXPORT void register_new_network_notify(struct global *, new_network_notify_fn, void *userdata);

/* hooks.c */
void server_disconnected_hook_execute(struct irc_network *);
void server_connected_hook_execute(struct irc_network *);
gboolean new_client_hook_execute(struct irc_client *c);
void lose_client_hook_execute(struct irc_client *c);
gboolean run_client_filter(struct irc_client *c, const struct irc_line *l, enum data_direction dir);
gboolean run_server_filter(struct irc_network *s, const struct irc_line *l, enum data_direction dir);
gboolean run_log_filter(struct irc_network *s, const struct irc_line *l, enum data_direction dir);
gboolean run_replication_filter(struct irc_network *s, const struct irc_line *l, enum data_direction dir);

/* client.c */
/**
 * Add a new client
 *
 * @param net Network this client is connected to. May be NULL.
 * @param io IO Channel to use for communication.
 * @param desc Description of the client.
 */
G_MODULE_EXPORT G_GNUC_MALLOC struct irc_client *client_init(
										   struct irc_network *net, 
										   GIOChannel *io, 
										   const char *desc);

/* log.c */
gboolean init_log(const char *file);
void log_network_line(const struct irc_network *n, const struct irc_line *l, gboolean incoming);
void log_client_line(const struct irc_client *c, const struct irc_line *l, gboolean incoming);
void handle_network_log(enum log_level level, const struct irc_network *n, 
						const char *msg);

/* redirect.c */
void redirect_record(const struct irc_network *n, const struct irc_client *c, const struct irc_line *l);
gboolean redirect_response(struct irc_network *n, const struct irc_line *l);
void redirect_clear(const struct irc_network *n);

/* cache.c */
gboolean client_try_cache(struct irc_client *c, struct irc_network_state *n, struct irc_line *l);

/* linestack.c */
void init_linestack(struct ctrlproxy_config *);

/* gen_config.c */
void network_update_config(struct irc_network_state *ns, struct network_config *nc);
void channel_update_config(struct irc_channel_state *ns, struct channel_config *nc);
void global_update_config(struct global *my_global);

/* repl.c */
void client_replicate(struct irc_client *);

gboolean init_replication(void);

/* main.c */
void free_global(struct global *);
void config_load_notify(struct global *global);
void config_save_notify(struct global *global, const char *);
struct global *load_global(const char *config_dir);
struct global *init_global(void);

/* nickserv.c */
void init_nickserv(void);
gboolean nickserv_load(struct global *global);
gboolean nickserv_save(struct global *global, const char *dir);
void nickserv_identify_me(struct irc_network *network, char *nick);

/* admin.c */
void init_admin(void);
gboolean admin_process_command(struct irc_client *c, struct irc_line *l, int cmdoffset);
void admin_log(enum log_level level, const struct irc_network *n, const struct irc_client *c, const char *data);
gboolean start_admin_socket(struct global *global);
gboolean stop_admin_socket(struct global *global);
gboolean admin_socket_prompt(const char *config_dir);

/* settings.c */
gboolean create_configuration(const char *config_dir);

/* pipes.c */
gboolean start_unix_socket(struct global *);
gboolean stop_unix_socket(struct global *);

/* log_custom.c */
void log_custom_load(struct log_file_config *config);

/* listener.c */
G_GNUC_MALLOC G_MODULE_EXPORT struct irc_listener *listener_init(struct global *global, struct listener_config *);
void free_listeners(struct global *global);

/* auto_away.c */
void auto_away_add(struct global *global, struct auto_away_config *config);



#endif /* __INTERNALS_H__ */

