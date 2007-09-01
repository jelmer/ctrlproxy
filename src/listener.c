/* 
	ctrlproxy: A modular IRC proxy
	(c) 2005-2006 Jelmer Vernooij <jelmer@nl.linux.org>
	
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

#include "internals.h"
#include "irc.h"
#include "listener.h"
#include "ssl.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <glib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#include <netdb.h>

#define DEFAULT_PORT "6667"

static GIConv iconv = (GIConv)-1;

static gboolean handle_client_receive(GIOChannel *c, GIOCondition condition, gpointer data) 
{
	struct line *l;
	struct listener *listener = data;
	GError *error = NULL;
	GIOStatus status;

	g_assert(c != NULL);

	while ((status = irc_recv_line(c, iconv, &error, &l)) == G_IO_STATUS_NORMAL) {
		g_assert(l != NULL);

		if (!l->args[0]){ 
			free_line(l);
			continue;
		}

		if (listener->config->password == NULL) {
			log_network(LOG_WARNING, listener->network, "No password set, allowing client _without_ authentication!");
		}

		if (!g_strcasecmp(l->args[0], "PASS")) {
			char *desc;
			struct network *n = listener->network;
			gboolean authenticated = FALSE;

			if (listener->config->password == NULL) {
				authenticated = TRUE;
			} else if (strcmp(l->args[1], listener->config->password) == 0) {
				authenticated = TRUE;
			} else if (strncmp(l->args[1], listener->config->password, strlen(listener->config->password)) == 0 &&
					   l->args[1][strlen(listener->config->password)+1] == ':') {
				authenticated = TRUE;
				n = find_network(listener->global, l->args[1]+strlen(listener->config->password)+1);
				if (n == NULL) {
					log_network(LOG_WARNING, listener->network, "User tried to log in with incorrect password!");
					irc_sendf(c, iconv, NULL, ":%s %d %s :Password mismatch", get_my_hostname(), ERR_PASSWDMISMATCH, "*");
	
					free_line(l);
					return TRUE;
				}
			}

			if (authenticated) {
				log_network (LOG_INFO, n, "Client successfully authenticated");

				desc = g_io_channel_ip_get_description(c);
				client_init(n, c, desc);

				free_line(l); 
				return FALSE;
			} else {
				log_network(LOG_WARNING, listener->network, "User tried to log in with incorrect password!");
				irc_sendf(c, iconv, NULL, ":%s %d %s :Password mismatch", get_my_hostname(), ERR_PASSWDMISMATCH, "*");
	
				free_line(l);
				return TRUE;
			}
		} else {
			irc_sendf(c, iconv, NULL, ":%s %d %s :You are not registered", get_my_hostname(), ERR_NOTREGISTERED, "*");
		}

		free_line(l);
	}

	if (status != G_IO_STATUS_AGAIN)
		return FALSE;
	
	return TRUE;
}

static gboolean handle_new_client(GIOChannel *c_server, GIOCondition condition, void *_listener)
{
	struct listener *listener = _listener;
	GIOChannel *c;
	int sock = accept(g_io_channel_unix_get_fd(c_server), NULL, 0);

	if (sock < 0) {
		log_global(LOG_WARNING, "Error accepting new connection: %s", strerror(errno));
		return TRUE;
	}

	c = g_io_channel_unix_new(sock);

	if (listener->config->ssl) {
#ifdef HAVE_GNUTLS
		c = ssl_wrap_iochannel(c, SSL_TYPE_SERVER, 
							 NULL, listener->config->ssl_credentials);
		g_assert(c != NULL);
#else
		log_global(LOG_WARNING, "SSL support not available, not listening for SSL connection");
#endif
	}

	g_io_channel_set_close_on_unref(c, TRUE);
	g_io_channel_set_encoding(c, NULL, NULL);
	g_io_channel_set_flags(c, G_IO_FLAG_NONBLOCK, NULL);
	g_io_add_watch(c, G_IO_IN, handle_client_receive, listener);

	g_io_channel_unref(c);

	listener->pending = g_list_append(listener->pending, c);

	return TRUE;
}

static int next_autoport(struct global *global)
{
	return ++global->config->listener_autoport;
}

/**
 * Start a listener.
 *
 * @param l Listener to start.
 */
gboolean start_listener(struct listener *l)
{
	int sock = -1;
	const int on = 1;
	struct addrinfo *res, *all_res;
	int error;
	struct addrinfo hints;
	struct listener_iochannel *lio;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_ADDRCONFIG | AI_PASSIVE;

	g_assert(!l->active);

	error = getaddrinfo(l->config->address, l->config->port != NULL?l->config->port:DEFAULT_PORT, &hints, &all_res);
	if (error) {
		log_global(LOG_ERROR, "Can't get address for %s:%s", l->config->address?l->config->address:"", l->config->port);
		return FALSE;
	}

	for (res = all_res; res; res = res->ai_next) {
		GIOChannel *ioc;

		lio = g_new0(struct listener_iochannel, 1);

		if (getnameinfo(res->ai_addr, res->ai_addrlen, 
						lio->address, NI_MAXHOST, lio->port, NI_MAXSERV, 
						NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
			strcpy(lio->address, "");
			strcpy(lio->port, "");
		}

		sock = socket(PF_INET, SOCK_STREAM, 0);
		if (sock < 0) {
			log_global(LOG_ERROR, "error creating socket: %s", strerror(errno));
			close(sock);
			g_free(lio);
			continue;
		}

		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	
		if (bind(sock, res->ai_addr, res->ai_addrlen) < 0) {
			/* Don't warn when binding to the same address using IPv4 
			 * /and/ ipv6. */
			if (!l->active || errno != EADDRINUSE)  {
				log_global(LOG_ERROR, "bind to %s:%s failed: %s", 
						   l->config->address, l->config->port, 
						   strerror(errno));
			}
			close(sock);
			g_free(lio);
			continue;
		}

		if (listen(sock, 5) < 0) {
			log_global(LOG_ERROR, "error listening on socket: %s", 
						strerror(errno));
			close(sock);
			g_free(lio);
			continue;
		}

		ioc = g_io_channel_unix_new(sock);

		if (ioc == NULL) {
			log_global(LOG_ERROR, 
						"Unable to create GIOChannel for server socket");
			close(sock);
			g_free(lio);
			continue;
		}

		g_io_channel_set_close_on_unref(ioc, TRUE);

		g_io_channel_set_encoding(ioc, NULL, NULL);
		lio->watch_id = g_io_add_watch(ioc, G_IO_IN, handle_new_client, l);
		g_io_channel_unref(ioc);
		l->incoming = g_list_append(l->incoming, lio);

		log_network( LOG_INFO, l->network, "Listening on %s:%s", 
					 lio->address, lio->port);
		l->active = TRUE;
	}

	freeaddrinfo(all_res);
	
	return l->active;
}

gboolean stop_listener(struct listener *l)
{
	while (l->incoming != NULL) {
		struct listener_iochannel *lio = l->incoming->data;

		g_source_remove(lio->watch_id);
		
		log_global(LOG_INFO, "Stopped listening at %s:%s", lio->address, 
					 lio->port);
		g_free(lio);

		l->incoming = g_list_remove(l->incoming, lio);
	}

	return TRUE;
}

void free_listener(struct listener *l)
{
	l->global->listeners = g_list_remove(l->global->listeners, l);
	g_free(l);
}

struct listener *listener_init(struct global *global, struct listener_config *cfg)
{
	struct listener *l = g_new0(struct listener, 1);

	l->config = cfg;
	l->global = global;

	if (l->config->network != NULL) {
		l->network = find_network(global, l->config->network);
		if (l->network == NULL) {
			free_listener(l);
			return NULL;
		}
	}

	l->global->listeners = g_list_append(l->global->listeners, l);

	return l;
}

static void auto_add_listener(struct network *n, void *private_data)
{
	GList *gl;
	struct listener *l;
	struct listener_config *cfg;
	

	/* See if there is already a listener for n */
	for (gl = n->global->listeners; gl; gl = gl->next) {
		l = gl->data;

		if (l->network == n || l->network == NULL)
			return;
	}

	cfg = g_new0(struct listener_config, 1);
	cfg->network = g_strdup(n->config->name);
	cfg->port = g_strdup_printf("%d", next_autoport(n->global));
	l = listener_init(n->global, cfg);
	start_listener(l);
}

gboolean init_listeners(struct global *global)
{
	GList *gl;
	gboolean ret = TRUE;

	if (global->config->auto_listener)
		register_new_network_notify(global, auto_add_listener, NULL);

	for (gl = global->config->listeners; gl; gl = gl->next) {
		struct listener_config *cfg = gl->data;
		struct listener *l = listener_init(global, cfg);

		ret &= start_listener(l);
	}
	return ret;
}

void fini_listeners(struct global *global)
{
	GList *gl;
	for(gl = global->listeners; gl; gl = gl->next) {
		struct listener *l = gl->data;

		if (l->active) 
			stop_listener(l);
	}
}
