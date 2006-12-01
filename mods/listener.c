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

static GIConv iconv = (GIConv)-1;

static gboolean handle_client_receive(GIOChannel *c, GIOCondition condition, gpointer data) 
{
	struct line *l;
	struct listener *listener = data;
	GError *error = NULL;
	GIOStatus status;

	g_assert(c);

	while ((status = irc_recv_line(c, iconv, &error, &l)) == G_IO_STATUS_NORMAL) {
		g_assert(l);

		if (!l->args[0]){ 
			free_line(l);
			continue;
		}

		if(!listener->password) {
			log_network("listener", LOG_WARNING, listener->network, "No password set, allowing client _without_ authentication!");
		}

		if(!g_strcasecmp(l->args[0], "PASS")) {
			char *desc;
			if (listener->password && strcmp(l->args[1], listener->password)) {
				log_network("listener", LOG_WARNING, listener->network, "User tried to log in with incorrect password!");
				irc_sendf(c, iconv, NULL, ":%s %d %s :Password mismatch", get_my_hostname(), ERR_PASSWDMISMATCH, "*");
	
				free_line(l);
				return TRUE;
			}

			log_network ("listener", LOG_INFO, listener->network, "Client successfully authenticated");

			desc = g_io_channel_ip_get_description(c);
			client_init(listener->network, c, desc);

			free_line(l); 
			return FALSE;
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
		log_global("listener", LOG_WARNING, "Error accepting new connection: %s", strerror(errno));
		return TRUE;
	}

	c = g_io_channel_unix_new(sock);

	if (listener->ssl) {
#ifdef HAVE_GNUTLS
		c = ssl_wrap_iochannel(c, SSL_TYPE_SERVER, 
											 NULL, listener->ssl_credentials);
		g_assert(c != NULL);
#else
		log_global("listener", LOG_WARNING, "SSL support not available, not listening for SSL connection");
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

/* FIXME: Store in global struct somehow */
static GList *listeners = NULL;
static int autoport = 6667;
static gboolean auto_listener = FALSE;
static GKeyFile *keyfile = NULL;

static int next_autoport()
{
	return ++autoport;
}

gboolean start_listener(struct listener *l)
{
	int sock = -1;
	const int on = 1;
	struct addrinfo *res, *all_res;
	int error;
	struct addrinfo hints;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	error = getaddrinfo(l->address, l->port, &hints, &all_res);
	if (error) {
		log_global("listener", LOG_ERROR, "Can't get address for %s:%s", l->address?l->address:"", l->port);
		return FALSE;
	}

	for (res = all_res; res; res = res->ai_next) {
		sock = socket(PF_INET, SOCK_STREAM, 0);
		if (sock < 0) {
			log_global( "listener", LOG_ERROR, "error creating socket: %s", strerror(errno));
			freeaddrinfo(all_res);
			return FALSE;
		}

		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	
		if (bind(sock, res->ai_addr, res->ai_addrlen) < 0) {
			log_global( "listener", LOG_ERROR, "bind to port %s failed: %s", l->port, strerror(errno));
			freeaddrinfo(all_res);
			return FALSE;
		}
	}

	if (sock == -1) {
		log_global( "listener", LOG_ERROR, "Unable to connect");
		freeaddrinfo(all_res);
		return FALSE;
	}

	freeaddrinfo(all_res);
	
	if (listen(sock, 5) < 0) {
		log_global( "listener", LOG_ERROR, "error listening on socket: %s", strerror(errno));
		return FALSE;
	}

	l->incoming = g_io_channel_unix_new(sock);

	if (!l->incoming) {
		log_global( "listener", LOG_ERROR, "Unable to create GIOChannel for server socket");
		return FALSE;
	}

	g_io_channel_set_close_on_unref(l->incoming, TRUE);

	g_io_channel_set_encoding(l->incoming, NULL, NULL);
	l->incoming_id = g_io_add_watch(l->incoming, G_IO_IN, handle_new_client, l);
	g_io_channel_unref(l->incoming);

	log_network( "listener", LOG_INFO, l->network, "Listening on %s:%s", l->address?l->address:"", l->port);

	l->active = TRUE;

	return TRUE;
}

gboolean stop_listener(struct listener *l)
{
	log_global ( "listener", LOG_INFO, "Stopped listening at %s:%s", l->address?l->address:"", l->port);
	g_source_remove(l->incoming_id);
	return TRUE;
}

void free_listener(struct listener *l)
{
	g_free(l->password);
	g_free(l->port);
	g_free(l->address);
	listeners = g_list_remove(listeners, l);
	g_free(l);
}

struct listener *listener_init(const char *address, const char *port)
{
	struct listener *l = g_new0(struct listener, 1);

	l->address = address?g_strdup(address):NULL;
	l->port = g_strdup(port);

	if (l->port == NULL) 
		l->port = g_strdup("6667");

	listeners = g_list_append(listeners, l);

	return l;
}

static void update_config(struct global *global, const char *path)
{
	GList *gl;
	char *filename;
	GKeyFile *kf; 
	GError *error = NULL;
	char *default_password;

	default_password = g_key_file_get_string(global->config->keyfile, "listener", "password", NULL);

	g_key_file_set_boolean(global->config->keyfile, "listener", "auto", auto_listener);

	g_key_file_set_integer(global->config->keyfile, "listener", "autoport", autoport);

	filename = g_build_filename(path, "listener", NULL);
	
	if (!keyfile)
		keyfile = g_key_file_new();

	kf = keyfile;

	for (gl = listeners; gl; gl = gl->next) {
		struct listener *l = gl->data;
		char *tmp;

		if (!l->address) 
			tmp = g_strdup(l->port);
		else
			tmp = g_strdup_printf("%s:%s", l->address, l->port);

		if (l->password && !(default_password && strcmp(l->password, default_password) == 0)) 
			g_key_file_set_string(kf, tmp, "password", l->password);

		if (l->network) 
			g_key_file_set_string(kf, tmp, "network", l->network->name);

		g_key_file_set_boolean(kf, tmp, "ssl", l->ssl);

		g_free(tmp);
	}

	if (!g_key_file_save_to_file(kf, filename, &error)) {
		log_global("listener", LOG_WARNING, "Unable to save to \"%s\": %s", filename, error->message);
	}
	
	g_free(filename);
}

static void auto_add_listener(struct network *n, void *private_data)
{
	GList *gl;
	char *port;
	struct listener *l;

	/* See if there is already a listener for n */
	for (gl = listeners; gl; gl = gl->next) {
		l = gl->data;

		if (l->network == n || l->network == NULL)
			return;
	}

	port = g_strdup_printf("%d", next_autoport());
	l = listener_init(NULL, port);
	l->network = n;
	start_listener(l);
}

static void load_config(struct global *global)
{
	char *filename = g_build_filename(global->config->config_dir, "listener", NULL);
	int i;
	char **groups;
	gsize size;
	GKeyFile *kf;
	char *default_password;

	default_password = g_key_file_get_string(global->config->keyfile, "listener", "password", NULL);
	if (g_key_file_has_key(global->config->keyfile, "listener", "auto", NULL))
		auto_listener = g_key_file_get_boolean(global->config->keyfile, "listener", "auto", NULL);

	if (g_key_file_has_key(global->config->keyfile, "listener", "autoport", NULL))
		autoport = g_key_file_get_integer(global->config->keyfile, "listener", "autoport", NULL);

	if (auto_listener)
		register_new_network_notify(global, auto_add_listener, NULL);

	keyfile = kf = g_key_file_new();

	if (!g_key_file_load_from_file(kf, filename, G_KEY_FILE_KEEP_COMMENTS, NULL)) {
		g_free(filename);
		return;
	}
		
	groups = g_key_file_get_groups(kf, &size);

	for (i = 0; i < size; i++)
	{
		struct listener *l;
		char *address, *port, *tmp;
		
		tmp = g_strdup(groups[i]);
		port = strrchr(tmp, ':');
		if (port) {
			address = tmp;
			*port = '\0';
			port++;
		} else {
			port = tmp;
			address = NULL;
		}
			
		l = listener_init(address, port);

		g_free(tmp);

		l->password = g_key_file_get_string(kf, groups[i], "password", NULL);
		if (!l->password)
			l->password = default_password;

		if (g_key_file_has_key(kf, groups[i], "ssl", NULL))
			l->ssl = g_key_file_get_boolean(kf, groups[i], "ssl", NULL);

#ifdef HAVE_GNUTLS
		if (l->ssl)
			l->ssl_credentials = ssl_create_server_credentials(global, kf, groups[i]);
#endif

		if (g_key_file_has_key(kf, groups[i], "network", NULL)) {

			tmp = g_key_file_get_string(kf, groups[i], "network", NULL);
			l->network = find_network(global, tmp);
			if (!l->network) {
				log_global("listener", LOG_ERROR, "Unable to find network named \"%s\"", tmp);
			}
			g_free(tmp);
		}
			
		start_listener(l);
	}

	g_strfreev(groups);
	g_free(filename);
}

static void fini_plugin(void)
{
	while(listeners) {
		struct listener *l = listeners->data;
		if (l->active) 
			stop_listener(l);
		free_listener(l);
	}
}


void cmd_start_listener(struct client *c, char **args, void *userdata)
{
	char *b, *p;
	struct listener *l;

	if (!args[1]) {
		admin_out(c, "No port specified");
		return;
	}

	if (!args[2]) {
		admin_out(c, "No password specified");
		return;
	}

	b = g_strdup(args[1]);
	if ((p = strchr(b, ':'))) {
		*p = '\0';
		p++;
	} else {
		p = b;
		b = NULL;
	}

	l = listener_init(b, p);

	if (b) g_free(b); else g_free(p);

	l->password = g_strdup(args[2]);

	if (args[3]) {
		l->network = find_network(c->network->global, args[3]);
		if (l->network == NULL) {
			admin_out(c, "No such network `%s'", args[3]);
			free_listener(l);
			return;
		}
	}

	start_listener(l);
}

void cmd_stop_listener(struct client *c, char **args, void *userdata)
{
	GList *gl;
	char *b, *p;
	int i = 0;

	if (!args[0]) {
		admin_out(c, "No port specified");
		return;
	}

	b = g_strdup(args[1]);
	if ((p = strchr(b, ':'))) {
		*p = '\0';
		p++;
	} else {
		p = b;
		b = NULL;
	}

	for (gl = listeners; gl; gl = gl->next) {
		struct listener *l = gl->data;

		if (b && !l->address)
			continue;

		if (b && l->address && strcmp(b, l->address) != 0)
			continue;

		if (strcmp(p, l->port) != 0)
			continue;

		stop_listener(l);
		free_listener(l);
		i++;
	}

	if (b) g_free(b); else g_free(p);

	admin_out(c, "%d listeners stopped", i);
}

void cmd_list_listener(struct client *c, char **args, void *userdata)
{
	GList *gl;

	for (gl = listeners; gl; gl = gl->next) {
		struct listener *l = gl->data;

		admin_out(c, "%s:%s%s%s%s", l->address?l->address:"", l->port, l->network?" (":"", l->network?l->network->name:"", l->network?")":"");
	}
}

const static struct admin_command listener_commands[] = {
	{ "STARTLISTENER", cmd_start_listener, "[<address>:]<port> <password> [<network>]", "Add listener on specified port" },
	{ "STOPLISTENER", cmd_stop_listener, "[<address>:]<port>", "Stop listener on specified port" },
	{ "LISTLISTENER", cmd_list_listener, "", "Add new network with specified name" },
	{ NULL }
};

static gboolean init_plugin(void)
{
	int i;
	register_load_config_notify(load_config);
	register_save_config_notify(update_config);

	for (i = 0; listener_commands[i].name; i++)
		register_admin_command(&listener_commands[i]);

	atexit(fini_plugin);
	return TRUE;
}

struct plugin_ops plugin = {
	.name = "listener",
	.version = 0,
	.init = init_plugin,
};
