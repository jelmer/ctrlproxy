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

/* TODO This breaks:
 *    * One client connected. User does /AWAY msg.
 *    * Client quits.
 *    * Client rejoins. If client_limit=0, the connection is set as not away
 *      => there should be distinction between auto-away and client-away?
 */

#include "ctrlproxy.h"
#include "local.h"
#include <string.h>

/**
 * State data for auto-away.
 */
struct auto_away_data {
	struct auto_away_config *config;
	int max_idle_time;
 	time_t last_message;
	guint timeout_id;
	struct global *global;
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
			    (d->config->client_limit < 0 || g_list_length(s->clients) <= d->config->client_limit)) {
				network_send_args(s, "AWAY", 
								  d->config->message != NULL?d->config->message:"Auto Away", 
								  NULL);
				if (d->config->nick != NULL) {
					network_send_args(s, "NICK", d->config->nick, NULL);
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

	if (d->config->client_limit >= 0 && d->config->client_limit < g_list_length(c->network->clients)+1)
		network_send_args(c->network, "AWAY", NULL);

	return TRUE;
}

void auto_away_add(struct global *global, struct auto_away_config *config)
{
	struct auto_away_data *d = g_new0(struct auto_away_data, 1);

	d->config = config;
	d->last_message = time(NULL);
	d->max_idle_time = config->max_idle_time != -1?config->max_idle_time:AUTO_AWAY_DEFAULT_TIME;
	d->timeout_id = g_timeout_add(1000, check_time, d);
	d->global = global;
	add_new_client_hook("auto-away", new_client, d);
	add_server_filter("auto-away", log_data, d, -1);
}
