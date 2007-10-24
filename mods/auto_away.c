/* 
	ctrlproxy: A modular IRC proxy
	(c) 2002-2003 Jelmer Vernooij <jelmer@nl.linux.org>

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

#include "ctrlproxy.h"
#include <string.h>

#define DEFAULT_TIME (10 * 60)

struct auto_away_data {
 	time_t last_message;
	time_t max_idle_time;
	guint timeout_id;
	gboolean only_for_noclients;
	struct global *global;
	char *message;
	char *nick;
};

static gboolean check_time(gpointer user_data) 
{
	struct auto_away_data *d = (struct auto_away_data *)user_data;

	if (time(NULL) - d->last_message > d->max_idle_time) {
		GList *sl;
		for (sl = d->global->networks; sl; sl = sl->next) {
			struct network *s = (struct network *)sl->data;
			if (s->connection.state == NETWORK_CONNECTION_STATE_MOTD_RECVD &&
				s->state != NULL && !s->state->is_away && 
			    (!d->only_for_noclients || s->clients == NULL)) {
				network_send_args(s, "AWAY", 
								  d->message != NULL?d->message:"Auto Away", 
								  NULL);
				if (d->nick != NULL) {
					network_send_args(s, "NICK", d->nick, NULL);
				}
			}
		}
	}

	return TRUE;
}

static gboolean log_data(struct network *n, const struct line *l, 
						 enum data_direction dir, void *userdata) 
{
	struct auto_away_data *d = userdata;
	GList *sl;

	if (dir == TO_SERVER && !g_strcasecmp(l->args[0], "AWAY")) {
		d->last_message = time(NULL);
	}

	if (dir == TO_SERVER &&  
	   (!g_strcasecmp(l->args[0], "PRIVMSG") || 
		!g_strcasecmp(l->args[0], "NOTICE"))) {
		d->last_message = time(NULL);
		for (sl = d->global->networks; sl; sl = sl->next) {
			struct network *s = (struct network *)sl->data;
			if (s->connection.state == NETWORK_CONNECTION_STATE_MOTD_RECVD &&
				s->state != NULL && s->state->is_away)
				network_send_args(s, "AWAY", NULL);
		}
	}
	return TRUE;
}

static gboolean new_client(struct client *c, void *userdata) 
{
	struct auto_away_data *d = userdata;

	if (d->only_for_noclients) 
		network_send_args(c->network, "AWAY", NULL);

	return TRUE;
}

static void load_config(struct global *global)
{
	struct auto_away_data *d;
	GKeyFile *kf = global->config->keyfile;

	if (!g_key_file_has_group(kf, "auto-away")) {
		del_server_filter("auto-away");
		return;
	}

	d = g_new0(struct auto_away_data, 1);
	d->global = global;
	
	add_server_filter("auto-away", log_data, d, -1);

	d->last_message = time(NULL);
	d->message = g_key_file_get_string(kf, "auto-away", "message", NULL);
	d->nick = g_key_file_get_string(kf, "auto-away", "nick", NULL);
	if (g_key_file_has_key(kf, "auto-away", "only_noclient", NULL))
		d->only_for_noclients = g_key_file_get_boolean(kf, "auto-away", "only_noclient", NULL);
	if (g_key_file_has_key(kf, "auto-away", "time", NULL))
		d->max_idle_time = g_key_file_get_integer(kf, "auto-away", "time", NULL);
	else
		d->max_idle_time = DEFAULT_TIME;

	d->timeout_id = g_timeout_add(1000, check_time, d);
	add_new_client_hook("auto-away", new_client, d);
}

static gboolean init_plugin() 
{
	register_load_config_notify(load_config);
	return TRUE;
}

struct plugin_ops plugin = {
	.name = "auto_away",
	.version = CTRLPROXY_PLUGIN_VERSION,
	.init = init_plugin,
};
