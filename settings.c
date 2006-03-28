/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2004 Jelmer Vernooij <jelmer@nl.linux.org>

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

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#define __USE_POSIX
#include <netdb.h>
#include <sys/socket.h>

static void config_save_tcp_servers(struct network_config *n, GKeyFile *kf)
{
	GList *gl;
	int i = 0; 
	gchar **values = g_new0(gchar *, g_list_length(n->type_settings.tcp_servers)+1);
	
	for (gl = n->type_settings.tcp_servers; gl; gl = gl->next) {
		struct tcp_server_config *ts = gl->data;
		char *name = g_strdup_printf("%s:%s", ts->host, ts->port);

		values[i] = name;

		g_key_file_set_boolean(kf, name, "ssl", ts->ssl);
		if (ts->password)
			g_key_file_set_string(kf, name, "password", ts->password);
		else
			g_key_file_remove_key(kf, name, "password", NULL);

		i++;
	}

	g_key_file_set_string_list(kf, NULL, "servers", values, i);

	g_strfreev(values);
}

static void config_save_network(const char *dir, struct network_config *n)
{
	GList *gl;
	GKeyFile *kf;

	if (!n->keyfile) {
		n->keyfile = g_key_file_new();
	}

	kf = n->keyfile;

	g_key_file_set_boolean(kf, NULL, "autoconnect", n->autoconnect);
	g_key_file_set_string(kf, NULL, "fullname", n->fullname);
	g_key_file_set_string(kf, NULL, "nick", n->nick);
	g_key_file_set_string(kf, NULL, "username", n->username);

	switch(n->type) {
	case NETWORK_VIRTUAL:
		g_key_file_set_string(kf, NULL, "virtual", n->type_settings.virtual_type);
		break;
	case NETWORK_PROGRAM:
		g_key_file_set_string(kf, NULL, "program", n->type_settings.program_location);
		break;
	case NETWORK_TCP:
		config_save_tcp_servers(n, kf);
		break;
	default:break;
	}
	
	for (gl = n->channels; gl; gl = gl->next) {
		struct channel_config *c = gl->data;

		if (c->key)
			g_key_file_set_string(kf, c->name, "key", c->key);
		else 
			g_key_file_remove_key(kf, c->name, "key", NULL);
		
		g_key_file_set_boolean(kf, c->name, "autojoin", c->autojoin);
	}

	/* FIXME: Save to file */
}

static void config_save_networks(const char *config_dir, GList *networks)
{
	char *networksdir = g_build_filename(config_dir, "networks", NULL);
	GList *gl;

	if (!g_file_test(networksdir, G_FILE_TEST_IS_DIR)) {
		if (mkdir(networksdir, 0700) != 0) {
			/* FIXME: ERROR */
			return;
		}
	}

	for (gl = networks; gl; gl = gl->next) {
		struct network_config *n = gl->data;		
		config_save_network(networksdir, n);
	}
}

void save_configuration(struct ctrlproxy_config *cfg, const char *configuration_file)
{
	if (!cfg->keyfile)
		cfg->keyfile = g_key_file_new();

	/* FIXME */

	config_save_networks(cfg->config_dir, cfg->networks);

	/* FIXME: Save to file */
}

static void config_load_channel(struct network_config *n, GKeyFile *kf, const char *name)
{
	struct channel_config *ch = g_new0(struct channel_config, 1);

	ch->name = g_strdup(name);
	if (g_key_file_has_key(kf, name, "key", NULL)) 
		ch->key = g_key_file_get_string(kf, name, "key", NULL);

	if (g_key_file_has_key(kf, name, "autojoin", NULL))
		ch->autojoin = g_key_file_get_boolean(kf, name, "autojoin", NULL);
	
	n->channels = g_list_append(n->channels, ch);
}

static void config_load_servers(struct network_config *n)
{
	gsize size;
	char **servers;
	int i;
	
	servers = g_key_file_get_string_list(n->keyfile, NULL, "servers", &size, NULL);

	if (!servers)
		return;

	for (i = 0; i < size; i++) {
		char *tmp;
		struct tcp_server_config *s = g_new0(struct tcp_server_config, 1);
		
		s->password = g_key_file_get_string(n->keyfile, servers[i], "password", NULL);
		if (g_key_file_has_key(n->keyfile, servers[i], "ssl", NULL))
			s->ssl = g_key_file_get_boolean(n->keyfile, servers[i], "ssl", NULL);

		tmp = strrchr(servers[i], ':');

		if (tmp) {
			*tmp = '\0';
			tmp++;
		}
		
		s->host = servers[i];
		s->port = g_strdup(tmp?tmp:"6667");

		n->type_settings.tcp_servers = g_list_append(n->type_settings.tcp_servers, s);
	}

	g_free(servers);
}

static struct network_config *config_load_network(struct ctrlproxy_config *cfg, const char *dirname, const char *name)
{
	GKeyFile *kf;
	struct network_config *n;
	char *filename;
	int i;
	char **groups;
	gsize size;

	n = network_config_init(cfg);
	kf = n->keyfile = g_key_file_new();

	filename = g_build_filename(dirname, name, NULL);

	if (!g_key_file_load_from_file(kf, filename, G_KEY_FILE_KEEP_COMMENTS, NULL)) {	
		/* FIXME: message */
		/* FIXME: free n */
		g_key_file_free(kf);
		return NULL;
	}	

	g_free(filename);

	n->autoconnect = (!g_key_file_has_key(kf, NULL, "autoconnect", NULL)) ||
					   g_key_file_get_boolean(kf, NULL, "autoconnect", NULL);

	if (g_key_file_has_key(kf, NULL, "fullname", NULL)) {
		n->fullname = g_key_file_get_string(kf, NULL, "fullname", NULL);
	}

	if (g_key_file_has_key(kf, NULL, "nick", NULL)) {
		n->nick = g_key_file_get_string(kf, NULL, "nick", NULL);
	}

	if (g_key_file_has_key(kf, NULL, "username", NULL)) {
		n->username = g_key_file_get_string(kf, NULL, "username", NULL);
	}

	if (g_key_file_has_key(kf, NULL, "ignore_first_nick", NULL)) {
		n->ignore_first_nick = g_key_file_get_boolean(kf, NULL, "ignore_first_nick", NULL);
	}

	if (g_key_file_has_key(kf, NULL, "password", NULL)) {
		n->password = g_key_file_get_string(kf, NULL, "password", NULL);
	}

	n->name = g_strdup(name);

	if (g_key_file_has_key(kf, NULL, "program", NULL)) 
		n->type = NETWORK_PROGRAM;
	else if (g_key_file_has_key(kf, NULL, "virtual", NULL)) 
		n->type = NETWORK_VIRTUAL;
	else 
		n->type = NETWORK_TCP;

	switch (n->type) {
	case NETWORK_TCP:
		config_load_servers(n);
		break;
	case NETWORK_PROGRAM:
		n->type_settings.program_location = g_key_file_get_string(kf, NULL, "program", NULL);
		break;
	case NETWORK_VIRTUAL:
		n->type_settings.virtual_type = g_key_file_get_string(kf, NULL, "virtual", NULL);
		break;
	}

	groups = g_key_file_get_groups(kf, &size);
	for (i = 0; i < size; i++) {
		if (is_channelname(groups[i], NULL))  /* FIXME: How about weird channel names ? */
			config_load_channel(n, kf, groups[i]);
	}

	g_strfreev(groups);

	return n;
}

static void config_load_networks(struct ctrlproxy_config *cfg)
{
	char *networksdir = g_build_filename(cfg->config_dir, "networks", NULL);
	GDir *dir;
	const char *name;

	dir = g_dir_open(networksdir, 0, NULL);
	if (!dir)
		return;

	while ((name = g_dir_read_name(dir))) {
		config_load_network(cfg, networksdir, name);
	}

	g_free(networksdir);

	g_dir_close(dir);
}

struct ctrlproxy_config *load_configuration(const char *file) 
{
	GKeyFile *kf;
	GError *error;
	struct ctrlproxy_config *cfg;

	cfg = g_new0(struct ctrlproxy_config, 1);

	kf = cfg->keyfile = g_key_file_new();

	if (!g_key_file_load_from_file(kf, file, G_KEY_FILE_KEEP_COMMENTS, &error)) {
		log_global(NULL, LOG_ERROR, "Can't parse configuration file '%s': %s", file, error->message);
		g_key_file_free(kf);
		g_free(cfg);
		return NULL;
	}

	/* FIXME */

	config_load_networks(cfg);

	return cfg;
}

struct network_config *network_config_init(struct ctrlproxy_config *cfg) 
{
	struct network_config *s = g_new0(struct network_config, 1);

	s->autoconnect = FALSE;
	s->nick = g_strdup(g_get_user_name());
	s->username = g_strdup(g_get_user_name());
	s->fullname = g_strdup(g_get_real_name());
	s->reconnect_interval = DEFAULT_RECONNECT_INTERVAL;

	if (cfg) 
		cfg->networks = g_list_append(cfg->networks, s);
	return s;
}

void free_config(struct ctrlproxy_config *cfg)
{
	while (cfg->networks) {
		struct network_config *nc = cfg->networks->data;
		g_free(nc->name);
		g_free(nc->nick);
		g_free(nc->fullname);
		g_free(nc->username);
		g_free(nc->password);
		while (nc->channels) {
			struct channel_config *cc = nc->channels->data;
			g_free(cc->name);
			g_free(cc->key);
			nc->channels = g_list_remove(nc->channels, cc);	
			g_free(cc);
		}
		switch (nc->type) {
		case NETWORK_TCP: 
			while (nc->type_settings.tcp_servers) {
				struct tcp_server_config *tc = nc->type_settings.tcp_servers->data;
				g_free(tc->host);
				g_free(tc->port);
				g_free(tc->password);
				nc->type_settings.tcp_servers = g_list_remove(nc->type_settings.tcp_servers, tc);
				g_free(tc);
			}
			break;
		case NETWORK_VIRTUAL:
			g_free(nc->type_settings.virtual_type);
			break;
		case NETWORK_PROGRAM:
			g_free(nc->type_settings.program_location);
			break;
		}
		cfg->networks = g_list_remove(cfg->networks, nc);
		g_free(nc);
	}
	g_free(cfg);
}
