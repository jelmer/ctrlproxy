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
#include "gettext.h"
#define _(s) gettext(s)

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
			if(!irccmp(n, name, nick)) { 
				xmlFree(name);
				*pass = xmlGetProp(cur, "password");
				return 1;
			}
			xmlFree(name);
		}
		cur = cur->next;
	}
	return 0;
}

static char *nickserv_nick(struct network *n)
{
	xmlNodePtr cur = nickserv_node(n);
	if(!cur || !xmlHasProp(cur, "nick")) return g_strdup("NickServ");

	return xmlGetProp(cur, "nick");
}

static void identify_me(struct network *network, char *nick)
{
	char *pass;
	if(nickserv_find_nick(network, nick, &pass)) {
		char *nickserv_n = nickserv_nick(network), *raw;
		asprintf(&raw, "IDENTIFY %s", pass);
		g_free(pass);
		irc_send_args(network->outgoing, "PRIVMSG", nickserv_n, raw, NULL);
		g_free(raw);
		xmlFree(nickserv_n);
	}
}

static gboolean log_data(struct line *l) {
	char *pass;
	static char *nickattempt = NULL;

	/* User has changed his/her nick. Check whether this nick needs to be identified */
	if(l->direction == FROM_SERVER && !g_strcasecmp(l->args[0], "NICK")) {
		identify_me(l->network, l->args[1]);
	}

	/* Keep track of the last nick that the user tried to take */
	if(l->direction == TO_SERVER && !g_strcasecmp(l->args[0], "NICK")) {
		if(nickattempt) g_free(nickattempt);
		nickattempt = g_strdup(l->args[1]);
	}

	/* If we receive a nick-already-in-use message, ghost the current user */
	if(l->direction == FROM_SERVER && atol(l->args[0]) == ERR_NICKNAMEINUSE) {
		if(nickattempt && nickserv_find_nick(l->network, nickattempt, &pass)) {
			char *nickserv_n = nickserv_nick(l->network), *raw, *netname;
			netname = xmlGetProp(l->network->xmlConf, "name");
			
			g_message(_("Ghosting current user using '%s' on %s"), nickattempt, netname);
			xmlFree(netname);

			asprintf(&raw, "GHOST %s %s", nickattempt, pass);
			irc_send_args(l->network->outgoing, "PRIVMSG", nickserv_n, raw, NULL);
			g_free(raw);
			xmlFree(nickserv_n);
			irc_send_args(l->network->outgoing, "NICK", nickattempt);
		}
	}

	return TRUE;
}

static void conned_data(struct network *n)
{
	char *nick;
	nick = xmlGetProp(n->xmlConf, "nick");
	identify_me(n, nick);
	xmlFree(nick);
}

gboolean fini_plugin(struct plugin *p) {
	del_server_connected_hook("nickserv");
	del_filter(log_data);
	return TRUE;
}

const char name_plugin[] = "nickserv";

gboolean init_plugin(struct plugin *p) {
	add_server_connected_hook("nickserv", conned_data);
	add_filter("nickserv", log_data);
	return TRUE;
}
