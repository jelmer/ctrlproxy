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
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <glib.h>


static void default_log_fn(enum log_level l, const struct irc_listener *listener, const char *ret)
{
	if (listener->network != NULL)
		network_log(l, listener->network, "%s", ret);
	else
		log_global(l, "%s", ret);
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

static gboolean handle_client_line(struct pending_client *pc, 
								   const struct irc_line *l)
{
	if (l == NULL || l->args[0] == NULL) { 
		return TRUE;
	}

	if (!g_strcasecmp(l->args[0], "PASS")) {
		char *desc;
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
											 networkname, 6667, TRUE);
				if (n == NULL) {
					irc_sendf(pc->connection, pc->listener->iconv, NULL, ":%s %d %s :Password error: unable to find network", 
							  get_my_hostname(), ERR_PASSWDMISMATCH, "*");
					return FALSE;
				}

				if (n->connection.state == NETWORK_CONNECTION_STATE_NOT_CONNECTED && 
					!connect_network(n)) {
					irc_sendf(pc->connection, pc->listener->iconv, NULL, ":%s %d %s :Password error: unable to connect", 
							  get_my_hostname(), ERR_PASSWDMISMATCH, "*");
					return FALSE;
				}
			}

			desc = g_io_channel_ip_get_description(pc->connection);
			pc->listener->new_client(n, pc->connection, desc);
			g_free(desc);

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
		irc_sendf(pc->connection, pc->listener->iconv, NULL, ":%s %d %s :You are not registered", get_my_hostname(), ERR_NOTREGISTERED, "*");
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

static void listener_new_client(struct irc_network *n, GIOChannel *ioc, const char *description)
{
	client_init(n, ioc, description);
}

struct irc_listener *listener_init(struct global *global, struct listener_config *cfg)
{
	struct irc_listener *l = g_new0(struct irc_listener, 1);

	l->config = cfg;
	l->global = global;
	l->new_client = listener_new_client;
	l->iconv = (GIConv)-1;
	l->handle_client_line = handle_client_line;
	l->log_fn = default_log_fn;

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


