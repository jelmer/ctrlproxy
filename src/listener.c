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
#include "socks.h"
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

static gboolean kill_pending_client(struct pending_client *pc)
{
	pc->listener->pending = g_list_remove(pc->listener->pending, pc);

	g_source_remove(pc->watch_id);

	g_free(pc->clientname);

	g_free(pc);

	return TRUE;
}

static gboolean handle_client_receive(GIOChannel *c, GIOCondition condition, gpointer data) 
{
	struct line *l;
	struct pending_client *pc = data;
	GError *error = NULL;
	GIOStatus status;

	if (condition == G_IO_HUP) {
		kill_pending_client(pc);
		return FALSE;	
	}

	g_assert(c != NULL);

	while ((status = irc_recv_line(c, iconv, &error, &l)) == G_IO_STATUS_NORMAL) {
		g_assert(l != NULL);

		if (!l->args[0]){ 
			free_line(l);
			continue;
		}

		if (pc->listener->config->password == NULL) {
			log_network(LOG_WARNING, pc->listener->network, "No password set, allowing client _without_ authentication!");
		}

		if (!g_strcasecmp(l->args[0], "PASS")) {
			char *desc;
			struct network *n = pc->listener->network;
			gboolean authenticated = FALSE;

			if (pc->listener->config->password == NULL) {
				authenticated = TRUE;
			} else if (strcmp(l->args[1], pc->listener->config->password) == 0) {
				authenticated = TRUE;
			} else if (strncmp(l->args[1], pc->listener->config->password, strlen(pc->listener->config->password)) == 0 &&
					   l->args[1][strlen(pc->listener->config->password)+1] == ':') {
				authenticated = TRUE;
				n = find_network(pc->listener->global, l->args[1]+strlen(pc->listener->config->password)+1);
				if (n == NULL) {
					log_network(LOG_WARNING, pc->listener->network, "User tried to log in with incorrect password!");
					irc_sendf(c, iconv, NULL, ":%s %d %s :Password mismatch", get_my_hostname(), ERR_PASSWDMISMATCH, "*");
	
					free_line(l);
					return TRUE;
				}
			}

			if (authenticated) {
				log_network (LOG_INFO, n, "Client successfully authenticated");

				desc = g_io_channel_ip_get_description(c);
				client_init(n, c, desc);

				kill_pending_client(pc);

				free_line(l); 
				return FALSE;
			} else {
				log_network(LOG_WARNING, pc->listener->network, "User tried to log in with incorrect password!");
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
	struct pending_client *pc;
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
	pc = g_new0(struct pending_client, 1);
	pc->connection = c;
	pc->watch_id = g_io_add_watch(c, G_IO_IN | G_IO_HUP, handle_client_receive, pc);
	pc->listener = listener;

	g_io_channel_unref(c);

	listener->pending = g_list_append(listener->pending, pc);

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


/* TODO:
 *  - support for ipv4 and ipv6 atyp's
 *  - support for gssapi method
 *  - support for ident method
 */

static gboolean socks_reply(GIOChannel *ioc, guint8 err, guint8 atyp, guint8 data_len, gchar *data, guint16 port)
{
	gchar *header = g_new0(gchar, 7 + data_len);
	GIOStatus status;
	gsize read;

	header[0] = SOCKS_VERSION;
	header[1] = err;
	header[2] = 0x0; /* Reserved */
	header[3] = atyp;
	memcpy(header+4, data, data_len);
	*((guint16 *)(header+4+data_len)) = htons(port);

	status = g_io_channel_write_chars(ioc, header, 6 + data_len, &read, NULL);

	g_free(header);

	g_io_channel_flush(ioc, NULL);

	return (err == REP_OK);
}

static gboolean socks_error(GIOChannel *ioc, guint8 err)
{
	guint8 data = 0x0;
	return socks_reply(ioc, err, ATYP_FQDN, 1, (gchar *)&data, 0);
}

static gboolean anon_acceptable(struct pending_client *cl)
{
	return FALSE; /* Don't allow anonymous connects */
}

static gboolean pass_acceptable(struct pending_client *cl)
{
	return TRUE; /* FIXME: Check whether there is a password specified */
}

static gboolean pass_handle_data(struct pending_client *cl)
{
	GList *gl;
	gchar header[2];
	gsize read;
	GIOStatus status;
	gchar uname[0x100], pass[0x100];

	status = g_io_channel_read_chars(cl->connection, header, 2, &read, NULL);
	if (status != G_IO_STATUS_NORMAL) {
		return FALSE;
	}

	if (header[0] != SOCKS_VERSION && header[0] != 0x1) {
		log_global(LOG_WARNING, "Client suddenly changed socks uname/pwd version to %x", header[0]);
	 	return socks_error(cl->connection, REP_GENERAL_FAILURE);
	}

	status = g_io_channel_read_chars(cl->connection, uname, header[1], &read, NULL);
	if (status != G_IO_STATUS_NORMAL) {
		return FALSE;
	}

	uname[(guint8)header[1]] = '\0';

	status = g_io_channel_read_chars(cl->connection, header, 1, &read, NULL);
	if (status != G_IO_STATUS_NORMAL) {
		return FALSE;
	}

	status = g_io_channel_read_chars(cl->connection, pass, header[0], &read, NULL);
	if (status != G_IO_STATUS_NORMAL) {
		return FALSE;
	}

	pass[(guint8)header[0]] = '\0';

	header[0] = 0x1;
	header[1] = 0x0; /* set to non-zero if invalid */

	for (gl = cl->listener->config->allow_rules; gl; gl = gl->next)
	{
		struct allow_rule *r = gl->data;

		if (r->password == NULL || r->username == NULL) 
			continue;

		if (strcmp(r->username, uname)) 
			continue;

		if (strcmp(r->password, pass))
			continue;

		break;
	}

	header[1] = (gl == NULL);

	status = g_io_channel_write_chars(cl->connection, header, 2, &read, NULL);
	if (status != G_IO_STATUS_NORMAL) {
		return FALSE;
	} 

	g_io_channel_flush(cl->connection, NULL);

	if (header[1] == 0x0) {
		cl->socks.state = SOCKS_STATE_NORMAL;		
		return TRUE;
	} else {
		log_global(LOG_WARNING, "Password mismatch for user %s", uname);
		return FALSE;
	}
}

static struct socks_method {
	gint id;
	const char *name;
	gboolean (*acceptable) (struct pending_client *cl);
	gboolean (*handle_data) (struct pending_client *cl);
} socks_methods[] = {
	{ SOCKS_METHOD_NOAUTH, "none", anon_acceptable, NULL },
	{ SOCKS_METHOD_GSSAPI, "gssapi", NULL, NULL },
	{ SOCKS_METHOD_USERNAME_PW, "username/password", pass_acceptable, pass_handle_data },
	{ -1, NULL, NULL }
};

static gboolean handle_client_data (GIOChannel *ioc, GIOCondition o, gpointer data)
{
	struct pending_client *cl = data;
	GIOStatus status;
	int i;

	if (cl->socks.state == SOCKS_STATE_NEW) {
		gchar header[2];
		gchar methods[0x100];
		gsize read;
		status = g_io_channel_read_chars(ioc, header, 2, &read, NULL);
		if (status != G_IO_STATUS_NORMAL) {
			kill_pending_client(cl);
			return FALSE;
		}

		if (header[0] != SOCKS_VERSION) 
		{
			log_global(LOG_WARNING, "Ignoring client with socks version %d", header[0]);
			kill_pending_client(cl);
			return FALSE;
		}

		/* None by default */
		cl->socks.method = NULL;

		status = g_io_channel_read_chars(ioc, methods, header[1], &read, NULL);
		if (status != G_IO_STATUS_NORMAL) {
			kill_pending_client(cl);
			return FALSE;
		}
		for (i = 0; i < header[1]; i++) {
			int j;
			for (j = 0; socks_methods[j].id != -1; j++)
			{
				if (socks_methods[j].id == methods[i] && 
					socks_methods[j].acceptable &&
					socks_methods[j].acceptable(cl)) {
					cl->socks.method = &socks_methods[j];
					break;
				}
			}
		}

		header[0] = SOCKS_VERSION;
		header[1] = cl->socks.method?cl->socks.method->id:SOCKS_METHOD_NOACCEPTABLE;

		status = g_io_channel_write_chars(ioc, header, 2, &read, NULL);
		if (status != G_IO_STATUS_NORMAL) {
			kill_pending_client(cl);
			return FALSE;
		} 

		g_io_channel_flush(ioc, NULL);

		if (!cl->socks.method) {
			log_global(LOG_WARNING, "Refused client because no valid method was available");
			kill_pending_client(cl);
			return FALSE;
		}

		log_global(LOG_INFO, "Accepted socks client authenticating using %s", cl->socks.method->name);

		if (!cl->socks.method->handle_data) {
			cl->socks.state = SOCKS_STATE_NORMAL;
		} else {
			cl->socks.state = SOCKS_STATE_AUTH;
		}
	} else if (cl->socks.state == SOCKS_STATE_AUTH) {
		gboolean ret;
		ret = cl->socks.method->handle_data(cl);
		if (!ret) 
			kill_pending_client(cl);
		return ret;
	} else if (cl->socks.state == SOCKS_STATE_NORMAL) {
		gchar header[4];
		gsize read;

		status = g_io_channel_read_chars(ioc, header, 4, &read, NULL);
		if (status != G_IO_STATUS_NORMAL) {
			kill_pending_client(cl);
			return FALSE;
		}

		if (header[0] != SOCKS_VERSION) {
			log_global(LOG_WARNING, "Client suddenly changed socks version to %x", header[0]);
			kill_pending_client(cl);
		 	return socks_error(ioc, REP_GENERAL_FAILURE);
		}

		if (header[1] != CMD_CONNECT) {
			log_global(LOG_WARNING, "Client used unknown command %x", header[1]);
			kill_pending_client(cl);
			return socks_error(ioc, REP_CMD_NOT_SUPPORTED);
		}

		/* header[2] is reserved */
	
		switch (header[3]) {
			case ATYP_IPV4: 
				kill_pending_client(cl);
				return socks_error(ioc, REP_ATYP_NOT_SUPPORTED);

			case ATYP_IPV6:
				kill_pending_client(cl);
				return socks_error(ioc, REP_ATYP_NOT_SUPPORTED);

			case ATYP_FQDN:
				{
					char hostname[0x100];
					guint16 port;
					char *desc;
					struct network *result;
					
					status = g_io_channel_read_chars(ioc, header, 1, &read, NULL);
					status = g_io_channel_read_chars(ioc, hostname, header[0], &read, NULL);
					hostname[(guint8)header[0]] = '\0';

					status = g_io_channel_read_chars(ioc, header, 2, &read, NULL);
					port = ntohs(*(guint16 *)header);

					log_global(LOG_INFO, "Request to connect to %s:%d", hostname, port);

					result = find_network_by_hostname(cl->listener->global, hostname, port, TRUE);

					if (!result) {
						log_global(LOG_WARNING, "Unable to return network matching %s:%d", hostname, port);
						kill_pending_client(cl);
						return socks_error(ioc, REP_NET_UNREACHABLE);
					} 

					if (result->connection.state == NETWORK_CONNECTION_STATE_NOT_CONNECTED && 
						!connect_network(result)) {
						log_network(LOG_ERROR, result, "Unable to connect");
						kill_pending_client(cl);
						return socks_error(ioc, REP_NET_UNREACHABLE);
					}

					if (result->config->type == NETWORK_TCP) {
#ifdef HAVE_IPV6
						struct sockaddr_in6 *name6; 
#endif
						struct sockaddr_in *name4; 
						int atyp, len, port;
						gchar *data;

#ifdef HAVE_IPV6
						name6 = (struct sockaddr_in6 *)result->connection.data.tcp.local_name;
#endif
						name4 = (struct sockaddr_in *)result->connection.data.tcp.local_name;

						if (name4->sin_family == AF_INET) {
							atyp = ATYP_IPV4;
							data = (gchar *)&name4->sin_addr;
							len = 4;
							port = name4->sin_port;
#ifdef HAVE_IPV6
						} else if (name6->sin6_family == AF_INET6) {
							atyp = ATYP_IPV6;
							data = (gchar *)&name6->sin6_addr;
							len = 16;
							port = name6->sin6_port;
#endif
						} else {
							log_network(LOG_ERROR, result, "Unable to obtain local address for connection to server");
							kill_pending_client(cl);
							return socks_error(ioc, REP_NET_UNREACHABLE);
						}
							
						socks_reply(ioc, REP_OK, atyp, len, data, port); 
						
					} else {
						gchar *data = g_strdup("xlocalhost");
						data[0] = strlen(data+1);
						
						socks_reply(ioc, REP_OK, ATYP_FQDN, data[0]+1, data, 1025);
					}

					desc = g_io_channel_ip_get_description(ioc);
					client_init(result, ioc, desc);
					g_free(desc);

					kill_pending_client(cl);

					return FALSE;
				}
			default:
				kill_pending_client(cl);
				return socks_error(ioc, REP_ATYP_NOT_SUPPORTED);

		}
	}
	
	return TRUE;
}
