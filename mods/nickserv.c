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
#include "irc.h"
#include "gettext.h"
#define _(s) gettext(s)

struct nickserv_entry {
	const char *network;
	const char *nick;
	const char *pass;
};

static GList *nicks = NULL;

static const char *nickserv_find_nick(struct network *n, char *nick)
{
	GList *gl;
	for (gl = nicks; gl; gl = gl->next) {
		struct nickserv_entry *e = gl->data;

		if (g_strcasecmp(e->nick, nick)) 
			continue;

		if (!e->network) return e->pass;
		if (!g_strcasecmp(e->network, n->name)) return e->pass;
	}
	
	return NULL;
}

static const char *nickserv_nick(struct network *n)
{
	return "NickServ";
}

static void identify_me(struct network *network, char *nick)
{
	const char *pass;

	/* Don't try to identify if we're already identified */
	if (network->mymodes['R']) return;
	
	pass = nickserv_find_nick(network, nick);
	
	if (pass) {
		const char *nickserv_n = nickserv_nick(network);
		char *raw;
		raw = g_strdup_printf("IDENTIFY %s", pass);
		network_send_args(network, "PRIVMSG", nickserv_n, raw, NULL);
		g_free(raw);
	} else {
		g_message("Not identifying for %s, network %s; no entries found", nick, network->name);
	}
}

static gboolean log_data(struct line *l, void *userdata) {
	static char *nickattempt = NULL;

	/* User has changed his/her nick. Check whether this nick needs to be identified */
	if(l->direction == FROM_SERVER && !g_strcasecmp(l->args[0], "NICK") &&
	   !g_strcasecmp(nickattempt, l->args[1])) {
		identify_me(l->network, l->args[1]);
	}

	/* Keep track of the last nick that the user tried to take */
	if(l->direction == TO_SERVER && !g_strcasecmp(l->args[0], "NICK")) {
		if(nickattempt) g_free(nickattempt);
		nickattempt = g_strdup(l->args[1]);
	}

	if (l->direction == TO_SERVER && 
		(!g_strcasecmp(l->args[0], "PRIVMSG") || !g_strcasecmp(l->args[0], "NOTICE")) &&
		(!g_strcasecmp(l->args[1], nickserv_nick(l->network)) && !g_strncasecmp(l->args[2], "IDENTIFY ", strlen("IDENTIFY ")))) {
			struct nickserv_entry *e = g_new0(struct nickserv_entry, 1);
			e->nick = g_strdup(l->network->nick);
			e->pass = l->args[2] + strlen("IDENTIFY ");
			e->network = g_strdup(l->network->name);
		
			nicks = g_list_prepend(nicks, e);
			g_message("Caching password for nick %s on network %s", e->nick, e->network);
	}

	/* If we receive a nick-already-in-use message, ghost the current user */
	if(l->direction == FROM_SERVER && atol(l->args[0]) == ERR_NICKNAMEINUSE) {
		const char *pass = nickserv_find_nick(l->network, nickattempt);
		if(nickattempt && pass) {
			const char *nickserv_n = nickserv_nick(l->network);
			char *raw;
			
			g_message(_("Ghosting current user using '%s' on %s"), nickattempt, l->network->name);

			raw = g_strdup_printf("GHOST %s %s", nickattempt, pass);
			network_send_args(l->network, "PRIVMSG", nickserv_n, raw, NULL);
			g_free(raw);
			network_send_args(l->network, "NICK", nickattempt, NULL);
		}
	}

	return TRUE;
}

static void conned_data(struct network *n, void *userdata)
{
	identify_me(n, n->nick);
}

gboolean save_config(struct plugin *p, xmlNodePtr node)
{
	/* FIXME */
	return TRUE;	
}

gboolean load_config(struct plugin *p, xmlNodePtr node)
{
	xmlNodePtr cur;

	for (cur = node->children; cur; cur = cur->next)
	{
		struct nickserv_entry *e;
		if (cur->type != XML_ELEMENT_NODE) continue;

		if (!strcmp(cur->name, "nick")) {
			e = g_new0(struct nickserv_entry, 1);
			e->nick = xmlGetProp(cur, "nick");
			e->pass = xmlGetProp(cur, "password");
			e->network = xmlGetProp(cur, "network");
		
			nicks = g_list_append(nicks, e);
		}
	}

	return TRUE;
}

gboolean fini_plugin(struct plugin *p) {
	del_server_connected_hook("nickserv");
	del_server_filter("nickserv");
	return TRUE;
}

const char name_plugin[] = "nickserv";

gboolean init_plugin(struct plugin *p) {
	add_server_connected_hook("nickserv", conned_data, NULL);
	add_server_filter("nickserv", log_data, NULL, 1);
	return TRUE;
}
