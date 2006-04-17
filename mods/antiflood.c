/* 
	ctrlproxy: A modular IRC proxy
	(c) 2003 Jelmer Vernooij <jelmer@nl.linux.org>

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

#include "ctrlproxy.h"
#include <string.h>
#include <time.h>

#include <sys/time.h>

#define DEFAULT_QUEUE_SPEED 2

struct antiflood_data {
    GHashTable *antiflood_servers;
    int queue_speed;
};

struct network_data {
	struct network *network;
	time_t tv_last_message;
	GQueue *message_queue;
	guint timeout_id;
	unsigned long queue_speed;
};

static gboolean send_queue(gpointer user_data) 
{
	gpointer d;
	time_t now = time(NULL);
	struct network_data *sd = (struct network_data *)user_data;

	if (sd->tv_last_message < now) 
		sd->tv_last_message = now;
	
	while (sd->tv_last_message < now && (d = g_queue_pop_tail(sd->message_queue))) {
		struct line *l = (struct line *)d;

		network_send_line(sd->network, NULL, l);

		free_line(l);
		
		sd->tv_last_message += sd->queue_speed;
	}

	return TRUE;
}

static gboolean log_data(struct network *network, struct line *l, enum data_direction dir, void *userdata) 
{
    struct antiflood_data *dt = userdata;
	struct network_data *sd;
	time_t now;
	
	if (dir != TO_SERVER) return TRUE;

	/* Assume local networks don't do flood protection */
	if (network->config->type != NETWORK_TCP) return TRUE;

	/* Get data for this server from the hash */
	sd = g_hash_table_lookup(dt->antiflood_servers, network);

	now = time(NULL);
	
	if(!sd) {
		sd = g_new(struct network_data,1);
		sd->queue_speed = dt->queue_speed;
		sd->tv_last_message = 0;

		sd->timeout_id = g_timeout_add(1000, send_queue , sd);

		sd->message_queue = g_queue_new();
		sd->network = network;

		g_hash_table_insert(dt->antiflood_servers, network, sd);
	}

	if (sd->tv_last_message < now) 
		sd->tv_last_message = now;
	
	if (sd->tv_last_message - now > 10) {
		log_network("antiflood", LOG_INFO, network, "Queueing message (%s)", l->args[0]);
		/* Push it up the stack! */
		g_queue_push_head(sd->message_queue, linedup(l));
		return FALSE;
	} else {
		sd->tv_last_message += sd->queue_speed;
	}
		
	return TRUE;
}

static void free_antiflood_server(gpointer user_data)
{
	struct network_data *sd = (struct network_data *)user_data;
	g_queue_free(sd->message_queue);
	g_source_remove(sd->timeout_id);
	g_free(sd);
}

static void load_config(struct global *global)
{
    GKeyFile *kf = global->config->keyfile;
    struct antiflood_data *dt;
    if (!g_key_file_has_group(kf, "antiflood")) {
	    del_server_filter("antiflood");
        return;
    }

    dt = g_new0(struct antiflood_data, 1);

    dt->antiflood_servers = g_hash_table_new_full(NULL, NULL, NULL, free_antiflood_server);
    if (g_key_file_has_key(kf, "antiflood", "queue-speed", NULL))
        dt->queue_speed = g_key_file_get_integer(kf, "antiflood", "queue-speed", NULL);
    else 
        dt->queue_speed = DEFAULT_QUEUE_SPEED;

	add_server_filter("antiflood", log_data, dt, 2000);
}

static gboolean init_plugin(void) 
{
	register_load_config_notify(load_config);
	return TRUE;
}

struct plugin_ops plugin = {
	.name = "antiflood",
	.version = 0,
	.init = init_plugin,
};
