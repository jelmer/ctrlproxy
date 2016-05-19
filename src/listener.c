/* 
	ctrlproxy: A modular IRC proxy
	(c) 2005-2006 Jelmer Vernooij <jelmer@jelmer.uk>
	
	Manual listen on ports

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
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
#include "socks.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <glib.h>
#include <sys/un.h>

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif


void default_listener_log_fn(enum log_level l, const struct irc_listener *listener, const char *ret)
{
	if (listener->network != NULL)
		network_log(l, listener->network, "%s", ret);
	else
		log_global(l, "%s", ret);
}

#ifdef HAVE_GSSAPI
gboolean default_socks_gssapi(struct pending_client *cl, gss_name_t authn_name)
{
	/* FIXME: Check if principal matches own user name */
	return FALSE;
}
#endif

static gboolean listener_check_username_password(struct irc_listener *listener, 
												 const char *username, const char *password)
{
	GList *gl;

	if (listener->config != NULL) {
		for (gl = listener->config->allow_rules; gl; gl = gl->next)
		{
			struct allow_rule *r = gl->data;

			if (r->password == NULL || r->username == NULL) 
				continue;

			if (strcmp(r->username, username)) 
				continue;

			if (strcmp(r->password, password))
				continue;

			return TRUE;
		}

		if (listener->config->password != NULL) {
			return (strcmp(listener->config->password, password) == 0);
		}
	}

	if (listener->global->config->password != NULL) {
		return (strcmp(listener->global->config->password, password) == 0);
	}

	listener_log(LOG_WARNING, listener, "No password set, allowing client _without_ authentication!");
	return TRUE;
}


gboolean default_socks_auth_simple(struct pending_client *cl, const char *username, const char *password,
								   gboolean (*on_known)(struct pending_client *, gboolean))
{
	if (listener_check_username_password(cl->listener, username, password))
		return on_known(cl, TRUE);

	return on_known(cl, FALSE);
}

gboolean default_socks_connect_fqdn (struct pending_client *cl, const char *hostname, uint16_t port)
{
	char *desc;
	struct irc_network *result;
	struct network_config *nc;
	
	listener_log(LOG_INFO, cl->listener, "Request to connect to %s:%d", hostname, port);

	result = find_network_by_hostname(cl->listener->global, hostname, port, 
									  cl->listener->global->config->create_implicit, NULL);

	if (result == NULL) {
		listener_log(LOG_WARNING, cl->listener, "Unable to return network matching %s:%d", hostname, port);
		return listener_socks_error(cl, REP_NET_UNREACHABLE);
	} 

	if (result->connection.state == NETWORK_CONNECTION_STATE_NOT_CONNECTED && 
		!connect_network(result)) {
		network_log(LOG_ERROR, result, "Unable to connect");
		return listener_socks_error(cl, REP_NET_UNREACHABLE);
	}

	nc = result->private_data;

	if (nc->type == NETWORK_TCP) {
		struct sockaddr *name; 
		int atyp, len, port;
		gchar *data;

		name = (struct sockaddr *)result->connection.data.tcp.local_name;

		if (name != NULL && name->sa_family == AF_INET) {
			struct sockaddr_in *name4 = (struct sockaddr_in *)name;
			atyp = ATYP_IPV4;
			data = (gchar *)&name4->sin_addr;
			len = 4;
			port = name4->sin_port;
		} else if (name != NULL && name->sa_family == AF_INET6) {
			struct sockaddr_in6 *name6 = (struct sockaddr_in6 *)name;
			atyp = ATYP_IPV6;
			data = (gchar *)&name6->sin6_addr;
			len = 16;
			port = name6->sin6_port;
		} else {
			network_log(LOG_ERROR, result, "Unable to obtain local address for connection to server");
			return listener_socks_error(cl, REP_NET_UNREACHABLE);
		}
			
		listener_socks_reply(cl, REP_OK, atyp, len, data, port); 
		
	} else {
		gchar *data = g_strdup("xlocalhost");
		data[0] = strlen(data+1);
		
		listener_socks_reply(cl, REP_OK, ATYP_FQDN, data[0]+1, data, 1025);
	}

	desc = g_io_channel_ip_get_description(cl->connection);
	if (desc == NULL) {
		if (cl->listener == cl->listener->global->unix_domain_socket_listener) 
			desc = g_strdup("Unix domain socket socks client");
		else
			desc = g_strdup("socks client");
	}
	client_init_iochannel(result, cl->connection, desc);
	g_free(desc);

	return FALSE;
}

void listener_log(enum log_level l, const struct irc_listener *listener,
				 const char *fmt, ...)
{
	char *ret;
	va_list ap;

	if (listener->log_fn == NULL)
		return;

	g_assert(listener);
	g_assert(fmt);

	va_start(ap, fmt);
	ret = g_strdup_vprintf(fmt, ap);
	va_end(ap);

	listener->log_fn(l, listener, ret);

	g_free(ret);
}

static gboolean handle_client_line(struct pending_client *pc, const struct irc_line *l)
{
	struct irc_listener *listener = pc->listener;

	if (l == NULL || l->args[0] == NULL) { 
		return TRUE;
	}

	if (!g_strcasecmp(l->args[0], "PASS")) {
		const char *networkname = NULL;
		struct irc_network *n = listener->network;
		gboolean authenticated = FALSE;
		char *actual_password;

		if (strchr(l->args[1], ':') != NULL) {
			char *p = strchr(l->args[1], ':');
			actual_password = g_strndup(l->args[1], p-l->args[1]);
			networkname = p+1;
		} else {
			actual_password = g_strdup(l->args[1]);
			networkname = NULL;
		}

		authenticated = listener_check_username_password(listener, NULL, actual_password);

		g_free(actual_password);

		if (authenticated) {
			listener_log(LOG_INFO, listener, "Client successfully authenticated");

			if (networkname != NULL) {
				n = find_network_by_hostname(listener->global, 
											 networkname, 6667, listener->global->config->create_implicit,
											 NULL);
				if (n == NULL) {
					irc_sendf(pc->connection, listener->iconv, NULL, 
							  ":%s %d %s :Password error: unable to find network", 
							  get_my_hostname(), ERR_PASSWDMISMATCH, "*");
					g_io_channel_flush(pc->connection, NULL);
					return FALSE;
				}

				if (n->connection.state == NETWORK_CONNECTION_STATE_NOT_CONNECTED && 
					!connect_network(n)) {
					irc_sendf(pc->connection, listener->iconv, NULL, 
							  ":%s %d %s :Password error: unable to connect", 
							  get_my_hostname(), ERR_PASSWDMISMATCH, "*");
					g_io_channel_flush(pc->connection, NULL);
					return FALSE;
				}
			}

			irc_sendf(pc->connection, listener->iconv, NULL, 
					  "NOTICE AUTH :PASS OK");
			g_io_channel_flush(pc->connection, NULL);


			{
				char *desc = g_io_channel_ip_get_description(pc->connection);
				if (pc->listener == pc->listener->global->unix_domain_socket_listener) 
					desc = g_strdup("Unix domain socket client");
				else if (desc == NULL)
					desc = g_strdup("");
				client_init_iochannel(n, pc->connection, desc);
				g_free(desc);
			}

			return FALSE;
		} else {
			GIOStatus status;
			listener_log(LOG_WARNING, listener, 
						 "User tried to log in with incorrect password!");

			status = irc_sendf(pc->connection, listener->iconv, NULL, 
							   ":%s %d %s :Password mismatch", 
							   get_my_hostname(), ERR_PASSWDMISMATCH, "*");
			g_io_channel_flush(pc->connection, NULL);

			if (status != G_IO_STATUS_NORMAL) {
				return FALSE;
			}

			return TRUE;
		}
	} else {
		irc_sendf(pc->connection, listener->iconv, NULL, ":%s %d %s :You are not registered. Did you specify a password?", get_my_hostname(), ERR_NOTREGISTERED, "*");
		g_io_channel_flush(pc->connection, NULL);
	}

	return TRUE;
}


static int next_autoport(struct global *global)
{
	return ++global->config->listener_autoport;
}

void free_listener(struct irc_listener *l)
{
	l->global->listeners = g_list_remove(l->global->listeners, l);

	irc_network_unref(l->network);
	
	g_free(l);
}

static struct irc_listener_ops default_listener_ops = {
	.handle_client_line = handle_client_line,
	.socks_auth_simple = default_socks_auth_simple,
#ifdef HAVE_GSSAPI
	.socks_gssapi = default_socks_gssapi,
#endif
	.socks_connect_fqdn = default_socks_connect_fqdn,
};

struct irc_listener *listener_init(struct global *global, struct listener_config *cfg)
{
	struct irc_listener *l = g_new0(struct irc_listener, 1);

	l->config = cfg;
	l->ssl = cfg->ssl;
	l->ssl_credentials = cfg->ssl_credentials;
	g_assert(!l->ssl || l->ssl_credentials != NULL);
	l->global = global;
	l->ops = &default_listener_ops;
	l->iconv = (GIConv)-1;
	l->log_fn = default_listener_log_fn;

	if (l->config->network != NULL) {
		l->network = irc_network_ref(find_network(global->networks, l->config->network));
		if (l->network == NULL) {
			listener_log(LOG_WARNING, l, "Network `%s' for listener not found", l->config->network);
		}
	}

	l->global->listeners = g_list_append(l->global->listeners, l);

	return l;
}

static void auto_add_listener(struct irc_network *n, void *private_data)
{
	GList *gl;
	struct irc_listener *l;
	struct listener_config *cfg;
	struct network_config *nc = n->private_data;
	
	/* See if there is already a listener for n */
	for (gl = n->global->listeners; gl; gl = gl->next) {
		l = gl->data;

		if (l->network == n || l->network == NULL)
			return;
	}

	cfg = g_new0(struct listener_config, 1);
	cfg->network = g_strdup(nc->name);
	cfg->port = g_strdup_printf("%d", next_autoport(n->global));
	l = listener_init(n->global, cfg);
	listener_start_tcp(l, NULL, cfg->port);
}

gboolean init_listeners(struct global *global)
{
	GList *gl;
	gboolean ret = TRUE;

	if (global->config->auto_listener)
		register_new_network_notify(global, auto_add_listener, NULL);

	for (gl = global->config->listeners; gl; gl = gl->next) {
		struct listener_config *cfg = gl->data;
		struct irc_listener *l = listener_init(global, cfg);

		if (l != NULL) {
			ret &= listener_start_tcp(l, cfg->address, cfg->port);
		}
	}
	return ret;
}

void fini_listeners(struct global *global)
{
	GList *gl;
	for(gl = global->listeners; gl; gl = gl->next) {
		struct irc_listener *l = gl->data;

		if (l->active) 
			listener_stop(l);
	}
}

gboolean start_unix_domain_socket_listener(struct global *global)
{
	int sock;
	struct sockaddr_un un;
	struct irc_listener *l = g_new0(struct irc_listener, 1);
	GIOChannel *ioc;

	sock = socket(PF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) {
		log_global(LOG_ERROR, "error creating unix socket: %s", strerror(errno));
		g_free(l);
		return FALSE;
	}
	
	un.sun_family = AF_UNIX;
	strncpy(un.sun_path, global->config->network_socket, sizeof(un.sun_path));
	unlink(un.sun_path);

	if (bind(sock, (struct sockaddr *)&un, sizeof(un)) < 0) {
		log_global(LOG_ERROR, "unable to bind to %s: %s", un.sun_path, strerror(errno));
		g_free(l);
		return FALSE;
	}
	
	if (listen(sock, 5) < 0) {
		log_global(LOG_ERROR, "error listening on socket: %s", strerror(errno));
		g_free(l);
		return FALSE;
	}

	l->ops = &default_listener_ops;
	l->config = NULL;
	l->iconv = (GIConv)-1;
	l->ssl = FALSE;
	l->ssl_credentials = NULL;
	l->global = global;
	l->log_fn = default_listener_log_fn;

	ioc = g_io_channel_unix_new(sock);

	if (ioc == NULL) {
		log_global(LOG_ERROR, "Unable to create GIOChannel for unix server socket");
		return FALSE;
	}

	listener_add_iochannel(l, ioc, NULL, NULL);

	global->unix_domain_socket_listener = l;

	return TRUE;
}

gboolean stop_unix_domain_socket_listener(struct global *global)
{
	if (global->unix_domain_socket_listener != NULL)
		listener_stop(global->unix_domain_socket_listener);
	unlink(global->config->network_socket);
	return TRUE;
}
