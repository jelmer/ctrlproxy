/* 
	ctrlproxy: A modular IRC proxy
	(c) 2005-2006 Jelmer Vernooij <jelmer@nl.linux.org>
	
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

void default_listener_log_fn(enum log_level l, const struct irc_listener *listener, const char *ret)
{
	if (listener->network != NULL)
		network_log(l, listener->network, "%s", ret);
	else
		log_global(l, "%s", ret);
}

gboolean default_socks_auth_simple(struct pending_client *cl, const char *username, const char *password)
{
	GList *gl;

	for (gl = cl->listener->config->allow_rules; gl; gl = gl->next)
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

	if (cl->listener->config->password == NULL) {
		listener_log(LOG_WARNING, cl->listener, "No password set, allowing client _without_ authentication!");
		return TRUE;
	}

	if (strcmp(cl->listener->config->password, password) == 0) {
		return TRUE;
	}

	return FALSE;
}

gboolean default_socks_connect_fqdn (struct pending_client *cl, const char *hostname, uint16_t port)
{
	char *desc;
	struct irc_network *result;
	struct network_config *nc;
	
	listener_log(LOG_INFO, cl->listener, "Request to connect to %s:%d", hostname, port);

	result = find_network_by_hostname(cl->listener->global, hostname, port, 
									  cl->listener->global->config->create_implicit);

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
	if (l == NULL || l->args[0] == NULL) { 
		return TRUE;
	}

	if (!g_strcasecmp(l->args[0], "PASS")) {
		const char *networkname = NULL;
		struct irc_network *n = pc->listener->network;
		gboolean authenticated = FALSE;

		if (pc->listener->config->password == NULL) {
			listener_log(LOG_WARNING, pc->listener,
							"No password set, allowing client _without_ authentication!");
			authenticated = TRUE;
			networkname = l->args[1];
		} else if (strcmp(l->args[1], pc->listener->config->password) == 0) {
			authenticated = TRUE;
		} else if (strncmp(l->args[1], pc->listener->config->password, 
						   strlen(pc->listener->config->password)) == 0 &&
				   l->args[1][strlen(pc->listener->config->password)] == ':') {
			authenticated = TRUE;
			networkname = l->args[1]+strlen(pc->listener->config->password)+1;
		}

		if (authenticated) {
			listener_log(LOG_INFO, pc->listener, "Client successfully authenticated");

			if (networkname != NULL) {
				n = find_network_by_hostname(pc->listener->global, 
											 networkname, 6667, pc->listener->global->config->create_implicit);
				if (n == NULL) {
					irc_sendf(pc->connection, pc->listener->iconv, NULL, ":%s %d %s :Password error: unable to find network", 
							  get_my_hostname(), ERR_PASSWDMISMATCH, "*");
					g_io_channel_flush(pc->connection, NULL);
					return FALSE;
				}

				if (n->connection.state == NETWORK_CONNECTION_STATE_NOT_CONNECTED && 
					!connect_network(n)) {
					irc_sendf(pc->connection, pc->listener->iconv, NULL, ":%s %d %s :Password error: unable to connect", 
							  get_my_hostname(), ERR_PASSWDMISMATCH, "*");
					g_io_channel_flush(pc->connection, NULL);
					return FALSE;
				}
			}

			{
				char *desc = g_io_channel_ip_get_description(pc->connection);
				client_init_iochannel(pc->listener->network, pc->connection, desc);
				g_free(desc);
			}

			return FALSE;
		} else {
			GIOStatus status;
			listener_log(LOG_WARNING, pc->listener, 
						 "User tried to log in with incorrect password!");

			status = irc_sendf(pc->connection, pc->listener->iconv, NULL, 
							   ":%s %d %s :Password mismatch", 
							   get_my_hostname(), ERR_PASSWDMISMATCH, "*");

			if (status != G_IO_STATUS_NORMAL) {
				return FALSE;
			}

			return TRUE;
		}
	} else {
		irc_sendf(pc->connection, pc->listener->iconv, NULL, ":%s %d %s :You are not registered. Did you specify a password?", get_my_hostname(), ERR_NOTREGISTERED, "*");
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

	network_unref(l->network);
	
	g_free(l);
}

static struct irc_listener_ops default_listener_ops = {
	.handle_client_line = handle_client_line,
	.socks_auth_simple = default_socks_auth_simple,
	.socks_connect_fqdn = default_socks_connect_fqdn,
};

struct irc_listener *listener_init(struct global *global, struct listener_config *cfg)
{
	struct irc_listener *l = g_new0(struct irc_listener, 1);

	l->config = cfg;
	l->ssl = cfg->ssl;
	l->ssl_credentials = cfg->ssl_credentials;
	l->global = global;
	l->ops = &default_listener_ops;
	l->iconv = (GIConv)-1;
	l->log_fn = default_listener_log_fn;

	if (l->config->network != NULL) {
		l->network = network_ref(find_network(global->networks, l->config->network));
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
	listener_start(l, NULL, cfg->port);
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
			ret &= listener_start(l, cfg->address, cfg->port);
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


