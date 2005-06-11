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
#include <sys/socket.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "listener"

static gboolean handle_client_receive(GIOChannel *c, GIOCondition condition, gpointer data) {
	struct line *l;
	struct listener *listener = data;
	GError *error = NULL;
	GIOStatus status;

	status = irc_recv_line(c, &error, &l);

	if (status != G_IO_STATUS_NORMAL) {
		return FALSE;
	}
	
	if (!l) return TRUE;

	if (!l->args[0]){ 
		free_line(l);
		return TRUE;
	}

	if(!listener->password) {
		log_network("listener", listener->network, "No password set, allowing client _without_ authentication!");
	}

	if(!g_strcasecmp(l->args[0], "PASS")) {
		if (listener->password && strcmp(l->args[1], listener->password)) {
			log_network("listener", listener->network, "User tried to log in with incorrect password!");
			irc_sendf(c, ":%s %d %s :Password mismatch\r\n", listener->network->name, ERR_PASSWDMISMATCH, listener->network->state->me.nick);

			free_line(l);
			return TRUE;
		}

		log_network ("listener", listener->network, "Client successfully authenticated");

		new_client(listener->network, c, NULL);

		free_line(l); 
		return FALSE;
	} else {
		irc_sendf(c, ":%s 451 * :You are not registered\r\n", 
				  get_my_hostname());
	}

	free_line(l);
	return TRUE;
}

static gboolean handle_new_client(GIOChannel *c_server, GIOCondition condition, void *_listener)
{
	struct listener *listener = _listener;
	GIOChannel *c;

	c = g_io_channel_unix_new(accept(g_io_channel_unix_get_fd(c_server), NULL, 0));

	if (listener->ssl) {
		GIOChannel *nio = sslize(c, TRUE);
		if (!nio) {
			log_global("listener", "SSL support not available, not listening for SSL connection");
		} else {
			c = nio;
		}
	}

	g_io_channel_set_close_on_unref(c, TRUE);
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

	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		log_global( "listener", "error creating socket: %s", strerror(errno));
		return FALSE;
	}

	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(l->port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind (sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		log_global( "listener", "bind to port %d failed: %s", l->port, strerror(errno));
		return FALSE;
	}

	if (listen(sock, 5) < 0) {
		log_global( "listener", "error listening on socket: %s", strerror(errno));
		return FALSE;
	}

	l->incoming = g_io_channel_unix_new(sock);

	if (!l->incoming) {
		log_global( "listener", "Unable to create GIOChannel for server socket");
		return FALSE;
	}

	g_io_channel_set_close_on_unref(l->incoming, TRUE);

	g_io_channel_set_encoding(l->incoming, NULL, NULL);
	l->incoming_id = g_io_add_watch(l->incoming, G_IO_IN, handle_new_client, l);
	g_io_channel_unref(l->incoming);

	log_network( "listener", l->network, "Listening on port %d", l->port);

	l->active = TRUE;

	return TRUE;
}

gboolean stop_listener(struct listener *l)
{
	log_global ( "listener", "Stopping listener at port %d", l->port);
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

static gboolean save_config(struct plugin *p, xmlNodePtr conf)
{
	GList *gl;
	char *tmp;
	
	for (gl = listeners; gl; gl = gl->next) {
		struct listener *l = gl->data;
		xmlNodePtr n = xmlNewNode(NULL, "listen");
	
		xmlSetProp(n, "port", tmp = g_strdup_printf("%d", l->port));
		g_free(tmp);

		if (l->password) 
			xmlSetProp(n, "password", l->password);

		if (l->network) 
			xmlSetProp(n, "network", l->network->name);

		if (l->ssl) 
			xmlSetProp(n, "ssl", "1");

		xmlAddChild(conf, n);
	}

	return TRUE;
}

static gboolean load_config(struct plugin *p, xmlNodePtr conf)
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
		if (xmlHasProp(cur, "ssl")) 
			l->ssl = 1;

		if (xmlHasProp(cur, "network")) {
			tmp = xmlGetProp(cur, "network");
			l->network = find_network(tmp);
			if (!l->network) {
				log_global("listener", "Unable to find network named \"%s\"", tmp);
			}
			xmlFree(tmp);
		}
			
		start_listener(l);
	}

	return TRUE;
}

static gboolean init_plugin(struct plugin *p) 
{
	return TRUE;
}

static gboolean fini_plugin(struct plugin *p)
{
	while(listeners) {
		struct listener *l = listeners->data;
		if (l->active) 
			stop_listener(l);
		free_listener(l);
	}

	return TRUE; 
}

struct plugin_ops plugin = {
	.name = "listener",
	.version = 0,
	.init = init_plugin,
	.fini = fini_plugin,
	.load_config = load_config,
	.save_config = save_config
};
