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

#define _GNU_SOURCE
#include "ctrlproxy.h"
#include <string.h>
#include <time.h>
//#include <sys/time.h>

#ifdef _WIN32
#include <windows.h>
#include <winbase.h>
#define TIMEVAL DWORD
#define gettimeofday(a,b) {*a = GetTickCount();}
#define timersub(a,b,c) {*c = *b - *a; }
#define tvtolong(v) (*v)
#else
#define TIMEVAL struct timeval
#define tvtolong(v) ((v)->tv_sec * 1000 + (v)->tv_usec / 1000)
#endif

static GHashTable *antiflood_servers = NULL;

static struct plugin *this_plugin = NULL;

struct network_data {
	TIMEVAL tv_last_message;
	GQueue *message_queue;
	unsigned long queue_speed;
	guint timeout_id;
};

static gboolean send_queue(gpointer user_data) {
	gpointer d;
	struct network_data *sd;
	struct line *l;
	struct plugin *old_plugin = current_plugin;
	current_plugin = this_plugin;

	sd = (struct network_data *)user_data;
	d = g_queue_pop_tail(sd->message_queue);
	if(!d){ 
		current_plugin = old_plugin; 
		return TRUE;
	}
	l = (struct line *)d;

	irc_send_line(l->network->outgoing, l);
	free_line(l);

	gettimeofday(&sd->tv_last_message, NULL);
	current_plugin = old_plugin;
	
	return TRUE;
}

static gboolean log_data(struct line *l) {
	TIMEVAL tv, cmp;
	struct network_data *sd;
	if(l->direction == FROM_SERVER)return TRUE;

	memset(&tv, 0, sizeof(TIMEVAL));

	/* Get data for this server from the hash */
	sd = g_hash_table_lookup(antiflood_servers, l->network);

	if(!sd) {
		xmlNodePtr cur;
		sd = malloc(sizeof(struct network_data));
		sd->queue_speed = 0;

		cur = xmlFindChildByElementName(l->network->xmlConf, "queue_speed");
		if(cur) {
			char *contents = xmlNodeGetContent(cur);
			if(contents) {
				sd->queue_speed = atol(contents);
				xmlFree(contents);
			}
		}

		memset(&sd, 0, sizeof(TIMEVAL));
		if(sd->queue_speed) sd->timeout_id = g_timeout_add(sd->queue_speed, send_queue , sd);
		else sd->timeout_id = -1;

		sd->message_queue = g_queue_new();

		g_hash_table_insert(antiflood_servers, l->network, sd);
	}
	
	gettimeofday(&tv, NULL);

	timersub(&tv, &sd->tv_last_message, &cmp);

	if(sd->queue_speed > 0 && tvtolong(&cmp) < sd->queue_speed) {
		/* Push it up the stack! */
		g_queue_push_head(sd->message_queue, linedup(l));
		l->options |= LINE_DONT_SEND;
	} else {
		gettimeofday(&sd->tv_last_message, NULL);
	}
		
	return TRUE;
}

static void free_antiflood_servers(gpointer key, gpointer value, gpointer user_data)
{
	struct network_data *sd = (struct network_data *)value;
	g_queue_free(sd->message_queue);
	if(sd->timeout_id != -1)g_source_remove(sd->timeout_id);
	free(sd);
}

gboolean fini_plugin(struct plugin *p) {
	del_filter_ex("client", log_data);
	
	g_hash_table_foreach(antiflood_servers, free_antiflood_servers, NULL);
	g_hash_table_destroy(antiflood_servers);
	
	return TRUE;
}

const char name_plugin[] = "antiflood";

gboolean init_plugin(struct plugin *p) {
	this_plugin = p;
	add_filter_ex("antiflood", log_data, "client", 1);
	antiflood_servers = g_hash_table_new(NULL, NULL);
	
	return TRUE;
}
