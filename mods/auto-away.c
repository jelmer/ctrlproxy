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

#define _GNU_SOURCE
#include "ctrlproxy.h"
#include <string.h>
#include "gettext.h"
#define _(s) gettext(s)

static time_t last_message = 0, max_idle_time = 60 * 10;
static gboolean only_for_noclients = FALSE;
static gboolean is_away = FALSE;
static char *message = NULL;
static char *nick = NULL;
static struct plugin *this_plugin = NULL;

struct auto_away_data {
	guint timeout_id;
};

static gboolean check_time(gpointer user_data) {
	push_plugin(this_plugin);
	if(time(NULL) - last_message > max_idle_time && !is_away) { 
		GList *sl = get_network_list();
		is_away = TRUE;
		while(sl) {
			struct network *s = (struct network *)sl->data;
			if(!only_for_noclients || s->clients == NULL) {
				char *new_msg;
				asprintf(&new_msg, ":%s", message?message:_("Auto Away"));
				irc_send_args(s->outgoing, "AWAY", new_msg, NULL);
				g_free(new_msg);
				if(nick) irc_send_args(s->outgoing, "NICK", nick, NULL);
			}
			sl = g_list_next(sl);
		}
	}
	pop_plugin();

	return TRUE;
}

static gboolean log_data(struct line *l) {
	if(l->direction == TO_SERVER && !g_strcasecmp(l->args[0], "AWAY")) {
		if(l->args[1])is_away = TRUE;
		else is_away = FALSE;
	}

	if(l->direction == TO_SERVER &&  
	   (!g_strcasecmp(l->args[0], "PRIVMSG") || !g_strcasecmp(l->args[0], "NOTICE"))) {
		last_message = time(NULL);
		if(is_away) {
			GList *sl = get_network_list();
			while(sl) {
				struct network *s = (struct network *)sl->data;
				irc_send_args(s->outgoing, "AWAY", NULL);
				sl = g_list_next(sl);
			}
			is_away = FALSE;
		}
	}
	return TRUE;
}

gboolean fini_plugin(struct plugin *p) {
	struct auto_away_data *d = (struct auto_away_data *)p->data;

	del_filter(log_data);
	g_source_remove(d->timeout_id);

	g_free(d);
	return TRUE;
}

const char name_plugin[] = "auto-away";

gboolean init_plugin(struct plugin *p) {
	char *t = NULL;
	struct auto_away_data *d;
	xmlNodePtr cur;

	this_plugin = p;

	d = g_new(struct auto_away_data,1);
	
	add_filter("auto-away", log_data);
	
	cur = p->xmlConf->xmlChildrenNode;
	while(cur) {
		if(!xmlIsBlankNode(cur) && !strcmp(cur->name, "message")) { 
			message = xmlNodeGetContent(cur); 
			t = xmlGetProp(cur, "time"); 
		} else if(!xmlIsBlankNode(cur) && !strcmp(cur->name, "only_noclient")) {
			only_for_noclients = TRUE;
		} else if(!xmlIsBlankNode(cur) && !strcmp(cur->name, "nick")) {
			nick = xmlNodeGetContent(cur);
		}
		cur = cur->next;
	}

	if(t) {
		max_idle_time = atol(t);
		xmlFree(t);
	}
	last_message = time(NULL);

	d->timeout_id = g_timeout_add(1000, check_time, NULL);

	p->data = d;
	return TRUE;
}