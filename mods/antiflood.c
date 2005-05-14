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

static GHashTable *antiflood_servers = NULL;

static int default_queue_speed = 2;
static struct plugin *this_plugin = NULL;

struct network_data {
	time_t tv_last_message;
	GQueue *message_queue;
	guint timeout_id;
	unsigned long queue_speed;
};

static gboolean send_queue(gpointer user_data) {
	gpointer d;
	time_t now = time(NULL);
	struct network_data *sd = (struct network_data *)user_data;

	if (sd->tv_last_message < now) 
		sd->tv_last_message = now;
	
	while (sd->tv_last_message < now && (d = g_queue_pop_tail(sd->message_queue))) {
		struct line *l = (struct line *)d;

		network_send_line(l->network, l);

		free_line(l);
		
		sd->tv_last_message += sd->queue_speed;
	}

	return TRUE;
}

static gboolean log_data(struct line *l, void *userdata) {
	struct network_data *sd;
	time_t now;
	
	if (l->direction != TO_SERVER) return TRUE;

	/* Assume local networks don't do flood protection */
	if (l->network->type != NETWORK_TCP) return TRUE;

	/* Get data for this server from the hash */
	sd = g_hash_table_lookup(antiflood_servers, l->network);

	now = time(NULL);
	
	if(!sd) {
		sd = g_new(struct network_data,1);
		sd->queue_speed = default_queue_speed;
		sd->tv_last_message = 0;

		sd->timeout_id = g_timeout_add(1000, send_queue , sd);

		sd->message_queue = g_queue_new();

		g_hash_table_insert(antiflood_servers, l->network, sd);
	}

	if (sd->tv_last_message < now) 
		sd->tv_last_message = now;
	
	if (sd->tv_last_message - now > 10) {
		log_network("antiflood", l->network, "Queueing message (%s)", l->args[0]);
		/* Push it up the stack! */
		g_queue_push_head(sd->message_queue, linedup(l));
		l->options |= LINE_DONT_SEND;
		return TRUE;
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

static gboolean fini_plugin(struct plugin *p) {
	del_server_filter("antiflood");
	
	g_hash_table_destroy(antiflood_servers);
	
	return TRUE;
}

static gboolean save_config(struct plugin *p, xmlNodePtr node)
{
	char *tmp = g_strdup_printf("%d", default_queue_speed);
	xmlSetProp(node, "queue-speed", tmp);
	g_free(tmp);
	return TRUE;
}

static gboolean load_config(struct plugin *p, xmlNodePtr node)
{
	char *qs = xmlGetProp(node, "queue-speed");
	
	if (qs) default_queue_speed = atoi(qs);

	xmlFree(qs);

	return TRUE;
}

static gboolean init_plugin(struct plugin *p) {
	this_plugin = p;
	add_server_filter("antiflood", log_data, NULL, 2000);
	antiflood_servers = g_hash_table_new_full(NULL, NULL, NULL, free_antiflood_server);
	
	return TRUE;
}

struct plugin_ops plugin = {
	.name = "antiflood",
	.version = 0,
	.init = init_plugin,
	.fini = fini_plugin,
	.load_config = load_config,
	.save_config = save_config
};
