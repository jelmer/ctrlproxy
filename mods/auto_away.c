/* 
	ctrlproxy: A modular IRC proxy
	(c) 2002-2003 Jelmer Vernooij <jelmer@nl.linux.org>

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

static time_t last_message = 0, max_idle_time = 60 * 10;
static gboolean only_for_noclients = FALSE;
static gboolean is_away = FALSE;
static char *message = NULL;
static char *nick = NULL;

struct auto_away_data {
	guint timeout_id;
};

static gboolean check_time(gpointer user_data) {

	if(time(NULL) - last_message > max_idle_time && !is_away) { 
		GList *sl = get_network_list();
		is_away = TRUE;
		while(sl) {
			struct network *s = (struct network *)sl->data;
			if(!only_for_noclients || s->clients == NULL) {
				char *new_msg;
				new_msg = g_strdup_printf(":%s", message?message:"Auto Away");
				network_send_args(s, "AWAY", new_msg, NULL);
				g_free(new_msg);
				if(nick) network_send_args(s, "NICK", nick, NULL);
			}
			sl = g_list_next(sl);
		}
	}

	return TRUE;
}

static gboolean log_data(struct network *n, struct line *l, enum data_direction dir, void *userdata) {
	if(dir == TO_SERVER && !g_strcasecmp(l->args[0], "AWAY")) {
		if(l->args[1])is_away = TRUE;
		else is_away = FALSE;
	}

	if(dir == TO_SERVER &&  
	   (!g_strcasecmp(l->args[0], "PRIVMSG") || !g_strcasecmp(l->args[0], "NOTICE"))) {
		last_message = time(NULL);
		if(is_away) {
			GList *sl = get_network_list();
			while(sl) {
				struct network *s = (struct network *)sl->data;
				network_send_args(s, "AWAY", NULL);
				sl = g_list_next(sl);
			}
			is_away = FALSE;
		}
	}
	return TRUE;
}

static gboolean fini_plugin(struct plugin *p) {
	struct auto_away_data *d = (struct auto_away_data *)p->data;

	g_free(message);
	del_server_filter("auto-away");
	g_source_remove(d->timeout_id);

	g_free(d);
	return TRUE;
}

static gboolean load_config(struct plugin *p, xmlNodePtr node)
{
	struct auto_away_data *d = (struct auto_away_data *)p->data;
	xmlNodePtr cur;
	char *t = NULL;

	for (cur = node->xmlChildrenNode; cur; cur = cur->next )
	{
		if(!xmlIsBlankNode(cur) && !strcmp(cur->name, "message")) { 
			message = xmlNodeGetContent(cur); 
			t = xmlGetProp(cur, "time"); 
		} else if(!xmlIsBlankNode(cur) && !strcmp(cur->name, "only_noclient")) {
			only_for_noclients = TRUE;
		} else if(!xmlIsBlankNode(cur) && !strcmp(cur->name, "nick")) {
			nick = xmlNodeGetContent(cur);
		}
	}

	if(t) {
		max_idle_time = atol(t);
		xmlFree(t);
	}

	d->timeout_id = g_timeout_add(1000, check_time, p);
	return TRUE;
}

static gboolean init_plugin(struct plugin *p) {
	struct auto_away_data *d;

	d = g_new(struct auto_away_data,1);
	
	add_server_filter("auto-away", log_data, p, -1);
	
	last_message = time(NULL);

	p->data = d;
	return TRUE;
}

struct plugin_ops plugin = {
	.name = "auto_away",
	.version = 0,
	.init = init_plugin,
	.fini = fini_plugin,
	.load_config = load_config
};
