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
#include "irc.h"

static xmlNodePtr nickserv_node(struct network *n)
{
	xmlNodePtr cur = n->xmlConf->xmlChildrenNode;
	while(cur) {
		if(!strcmp(cur->name, "nickserv")) return cur;
		cur = cur->next;
	}

	return NULL;
}

static int nickserv_find_nick(struct network *n, char *nick, char **pass)
{
	xmlNodePtr p = nickserv_node(n);
	xmlNodePtr cur;
	if(!p) return 0;

	cur = p->xmlChildrenNode;
	while(cur) {
		if(!strcmp(cur->name, "nick") && xmlHasProp(cur, "name")) {
			char *name = xmlGetProp(cur, "name");
			if(!strcasecmp(name, nick)) { 
				xmlFree(name);
				*pass = xmlGetProp(cur, "password");
				return 1;
			}
		}
		cur = cur->next;
	}
	return 0;
}

static char *nickserv_nick(struct network *n)
{
	xmlNodePtr cur = nickserv_node(n);
	if(!cur || !xmlHasProp(cur, "nick")) return strdup("NickServ");

	return xmlGetProp(cur, "nick");
}

static gboolean log_data(struct line *l) {
	char *pass;
	static char *nickattempt = NULL;
	static gboolean last_line_was_user = FALSE;

	if(
		/* User has changed his/her nick. Check whether this nick needs to be identified */
	   (l->direction == FROM_SERVER && !strcasecmp(l->args[0], "NICK")) ||
		/* We're in a login sequence */
	   (last_line_was_user && l->direction == TO_SERVER && !strcasecmp(l->args[0], "NICK"))) {
		if(nickserv_find_nick(l->network, l->args[1], &pass)) {
			char *nickserv_n = nickserv_nick(l->network), *raw;
			asprintf(&raw, "IDENTIFY %s", pass);
			irc_send_args(l->network->outgoing, "PRIVMSG", nickserv_n, raw, NULL);
			free(raw);
			xmlFree(nickserv_n);
		}
	}

	last_line_was_user = FALSE;
	
	/* Used in if-statement above */
	if(l->direction == TO_SERVER && !strcasecmp(l->args[0], "USER")) 
		last_line_was_user = TRUE;

	/* Keep track of the last nick that the user tried to take */
	if(l->direction == TO_SERVER && !strcasecmp(l->args[0], "NICK")) {
		if(nickattempt) free(nickattempt);
		nickattempt = strdup(l->args[1]);
	}

	/* If we receive a nick-already-in-use message, ghost the current user */
	if(l->direction == FROM_SERVER && atol(l->args[0]) == ERR_NICKNAMEINUSE) {
		if(nickattempt && nickserv_find_nick(l->network, nickattempt, &pass)) {
			char *nickserv_n = nickserv_nick(l->network), *raw;
			asprintf(&raw, "GHOST %s %s", nickattempt, pass);
			irc_send_args(l->network->outgoing, "PRIVMSG", nickserv_n, raw, NULL);
			free(raw);
			xmlFree(nickserv_n);
		}
	}

	return TRUE;
}

gboolean fini_plugin(struct plugin *p) {
	del_filter(log_data);
	return TRUE;
}

gboolean init_plugin(struct plugin *p) {
	add_filter("nickserv", log_data);
	return TRUE;
}
