/* 
	ctrlproxy: A modular IRC proxy
	noticelog: Send ctrlproxy log messages to the user using NOTICE's
	
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

static void admin_log(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data) 
{
	struct network *n;
	char *server_name, *nick;
	struct line *l;
	if(!networks)return;
	n = (struct network *)networks->data;
	server_name = xmlGetProp(n->xmlConf, "name");
	nick = xmlGetProp(n->xmlConf, "nick");
	l = irc_parse_linef(":ctrlproxy!ctrlproxy@%s NOTICE %s :%s", server_name, nick, message);
	clients_send(n, l, NULL);
	free_line(l);
	xmlFree(server_name);
	xmlFree(nick);
}

gboolean fini_plugin(struct plugin *p) {
	guint *id = p->data;
	g_log_remove_handler(NULL, *id);
	free(id);
	return TRUE;
}

gboolean init_plugin(struct plugin *p) {
	guint id = g_log_set_handler(NULL, G_LOG_LEVEL_MASK | G_LOG_FLAG_RECURSION, admin_log, NULL);
	p->data = malloc(sizeof(guint));
	memcpy(p->data, &id, sizeof(guint));
	return TRUE;
}
