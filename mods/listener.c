/* 
	ctrlproxy: A modular IRC proxy
	(c) 2005 Jelmer Vernooij <jelmer@nl.linux.org>
	
	Manual listen on ports

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
#include "irc.h"
#include "listener.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <glib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "listener"

static gboolean handle_client_receive(GIOChannel *c, GIOCondition condition, gpointer data) {
	struct line *l;
	struct listener *listener = data;

	l = irc_recv_line(c);

	if (!l) return TRUE;

	if(!l->args[0]){ 
		free_line(l);
		return TRUE;
	}

	if(!listener->password) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "No password set for %s, allowing client _without_ authentication!", listener->network->name);
	}

	if(!g_strcasecmp(l->args[0], "PASS")) {
		if (listener->password && strcmp(l->args[1], listener->password)) {
			g_warning("User tried to log in to %s with incorrect password!\n", listener->network->name);
			irc_sendf(c, ":%s %d %s :Password mismatch\r\n", listener->network->name, ERR_PASSWDMISMATCH, listener->network->nick);

			free_line(l);
			return FALSE;
		}

		g_message("Client successfully authenticated");

		new_client(listener->network, c);

		free_line(l); 
		return FALSE;
	} else {
		irc_sendf(c, ":%s 451 %s :You are not registered\r\n", 
				  listener->network->name,
				  listener->network->nick);
	}

	free_line(l);
	return TRUE;
}

static gboolean handle_new_client(GIOChannel *c_server, GIOCondition condition, void *_listener)
{
	struct listener *listener = _listener;
	GIOChannel *c;

	c = g_io_channel_unix_new(accept(g_io_channel_unix_get_fd(c_server), NULL, 0));

	g_io_add_watch(c, G_IO_IN, handle_client_receive, listener);

	g_io_channel_unref(c);

	listener->pending = g_list_append(listener->pending, c);

	return TRUE;
}

static GList *listeners = NULL;

gboolean start_listener(struct listener *l)
{
	int sock;
	const int on = 1;
	struct sockaddr_in addr;

	g_message("Starting listener at port %d", l->port);

	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		g_warning( "error creating socket: %s", strerror(errno));
		return FALSE;
	}

	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(l->port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind (sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		g_warning("Unable to bind to port %d: %s", l->port, strerror(errno));
		return FALSE;
	}

	if (listen(sock, 5) < 0) {
		g_warning( "error listening on socket: %s", strerror(errno));
		return FALSE;
	}

	l->incoming = g_io_channel_unix_new(sock);

	if (!l->incoming) {
		g_warning("Unable to create GIOChannel for server socket");
		return FALSE;
	}

	l->incoming_id = g_io_add_watch(l->incoming, G_IO_IN, handle_new_client, l);
	g_io_channel_unref(l->incoming);

	g_message("Listening for connections on port %d", l->port);

	return TRUE;
}

gboolean stop_listener(struct listener *l)
{
	g_message("Stopping listener at port %d", l->port);
	g_source_remove(l->incoming_id);
	return TRUE;
}

void free_listener(struct listener *l)
{
	g_free(l->password);
	listeners = g_list_remove(listeners, l);
	g_free(l);
}

struct listener *new_listener(guint16 port)
{
	struct listener *l = g_new0(struct listener, 1);
	l->port = port;

	listeners = g_list_append(listeners, l);

	return l;
}

gboolean load_config(struct plugin *p, xmlNodePtr conf)
{
	xmlNodePtr cur;

	for (cur = conf->children; cur; cur = cur->next)
	{
		struct listener *l;
		char *tmp;
		if (cur->type != XML_ELEMENT_NODE) continue;

		tmp = xmlGetProp(cur, "port");
		l = new_listener(atoi(tmp));
		xmlFree(tmp);

		l->password = xmlGetProp(cur, "password");

		if (xmlHasProp(cur, "network")) {
			tmp = xmlGetProp(cur, "network");
			l->network = find_network(tmp);
			xmlFree(tmp);
		}
			
		start_listener(l);
	}

	return TRUE;
}

gboolean init_plugin(struct plugin *p) 
{
	return TRUE;
}

gboolean fini_plugin(struct plugin *p)
{
	while(listeners) {
		struct listener *l = listeners->data;
		stop_listener(l);
		free_listener(l);
	}

	return TRUE; 
}
