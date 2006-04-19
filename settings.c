/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2006 Jelmer Vernooij <jelmer@nl.linux.org>

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

#include <netdb.h>
#include <sys/socket.h>
#include <glib/gstdio.h>

gboolean g_key_file_save_to_file(GKeyFile *kf, const gchar *file, GError **error)
{
	gsize length, nr;
	char *data = g_key_file_to_data(kf, &length, error);
	GIOChannel *gio;

	if (!data)
		return FALSE;

	gio = g_io_channel_new_file(file, "w+", error);
	if (!gio) {
		g_free(data);
		return FALSE;
	}
	
	g_io_channel_write_chars(gio, data, length, &nr, error);

	g_free(data);

	g_io_channel_unref(gio);

	return TRUE;
}

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

	g_key_file_set_string_list(kf, "global", "servers", (const gchar **)values, i);

	g_strfreev(values);
}

static void config_save_network(const char *dir, struct network_config *n)
{
	GList *gl;
	GKeyFile *kf;
	char *fn;
	
	if (!n->keyfile) {
		n->keyfile = g_key_file_new();
	}

	kf = n->keyfile;

	g_key_file_set_string(kf, "global", "fullname", n->fullname);
	g_key_file_set_string(kf, "global", "nick", n->nick);
	g_key_file_set_string(kf, "global", "username", n->username);
	g_key_file_set_integer(kf, "global", "reconnect-interval", n->reconnect_interval);

	switch(n->type) {
	case NETWORK_VIRTUAL:
		g_key_file_set_string(kf, "global", "virtual", n->type_settings.virtual_type);
		break;
	case NETWORK_PROGRAM:
		g_key_file_set_string(kf, "global", "program", n->type_settings.program_location);
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

	fn = g_build_filename(dir, n->name, NULL);
	g_key_file_save_to_file(kf, fn, NULL);
	g_free(fn);
}

static void config_save_networks(const char *config_dir, GList *networks)
{
	char *networksdir = g_build_filename(config_dir, "networks", NULL);
	GList *gl;

	if (!g_file_test(networksdir, G_FILE_TEST_IS_DIR)) {
		if (g_mkdir(networksdir, 0700) != 0) {
			log_global(NULL, LOG_ERROR, "Can't create networks directory '%s': %s", networksdir, strerror(errno));
			return;
		}
	}

	for (gl = networks; gl; gl = gl->next) {
		struct network_config *n = gl->data;		
		config_save_network(networksdir, n);
	}

	g_free(networksdir);
}

void save_configuration(struct ctrlproxy_config *cfg, const char *configuration_dir)
{
	char *fn, **list;
	int i;
	GList *gl;

	if (!g_file_test(configuration_dir, G_FILE_TEST_IS_DIR)) {
		if (g_mkdir(configuration_dir, 0700) != 0) {
			log_global(NULL, LOG_ERROR, "Unable to open configuration directory '%s'\n", configuration_dir);
			return;
		}
	}

	if (!cfg->keyfile)
		cfg->keyfile = g_key_file_new();

	g_key_file_set_boolean(cfg->keyfile, "global", "autosave", cfg->autosave);
	g_key_file_set_boolean(cfg->keyfile, "admin", "without_privmsg", cfg->admin_noprivmsg);

	if (cfg->replication)
		g_key_file_set_string(cfg->keyfile, "global", "replication", cfg->replication);
	if (cfg->linestack_backend) 
		g_key_file_set_string(cfg->keyfile, "global", "linestack", cfg->linestack_backend);
	g_key_file_set_string(cfg->keyfile, "global", "motd-file", cfg->motd_file);

	g_key_file_set_boolean(cfg->keyfile, "global", "report-time", cfg->report_time);

	config_save_networks(configuration_dir, cfg->networks);

	i = 0;
	list = g_new0(char *, g_list_length(cfg->networks)+1);
	for (gl = cfg->networks; gl; gl = gl->next) {
		struct network_config *nc = gl->data;

		if (nc->autoconnect) {
			list[i] = nc->name;
			i++;
		}
	}
	
	if (i > 0) 
		g_key_file_set_string_list(cfg->keyfile, "global", "autoconnect", (const gchar **)list, i);

	g_free(list);

	fn = g_build_filename(configuration_dir, "config", NULL);
	g_key_file_save_to_file(cfg->keyfile, fn, NULL);
	g_free(fn);
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
	
	servers = g_key_file_get_string_list(n->keyfile, "global", "servers", &size, NULL);

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
	GError *error = NULL;
	gsize size;

	kf = g_key_file_new();

	filename = g_build_filename(dirname, name, NULL);

	if (!g_key_file_load_from_file(kf, filename, G_KEY_FILE_KEEP_COMMENTS, &error)) {	
		log_global(NULL, LOG_ERROR, "Can't parse configuration file '%s': %s", filename, error->message);
		g_key_file_free(kf);
		return NULL;
	}	

	n = network_config_init(cfg);
	n->keyfile = kf;

	g_free(filename);

	if (g_key_file_has_key(kf, "global", "fullname", NULL)) {
		g_free(n->fullname);
		n->fullname = g_key_file_get_string(kf, "global", "fullname", NULL);
	}

	if (g_key_file_has_key(kf, "global", "nick", NULL)) {
		g_free(n->nick);
		n->nick = g_key_file_get_string(kf, "global", "nick", NULL);
	}

	if (g_key_file_has_key(kf, "global", "reconnect-interval", NULL)) {
		n->reconnect_interval = g_key_file_get_integer(kf, "global", "reconnect-interval", NULL);
	}

	if (g_key_file_has_key(kf, "global", "username", NULL)) {
		g_free(n->username);
		n->username = g_key_file_get_string(kf, "global", "username", NULL);
	}

	if (g_key_file_has_key(kf, "global", "ignore_first_nick", NULL)) {
		n->ignore_first_nick = g_key_file_get_boolean(kf, "global", "ignore_first_nick", NULL);
	}

	if (g_key_file_has_key(kf, "global", "password", NULL)) {
		g_free(n->password);
		n->password = g_key_file_get_string(kf, "global", "password", NULL);
	}

	n->name = g_strdup(name);

	if (g_key_file_has_key(kf, "global", "program", NULL)) 
		n->type = NETWORK_PROGRAM;
	else if (g_key_file_has_key(kf, "global", "virtual", NULL)) 
		n->type = NETWORK_VIRTUAL;
	else 
		n->type = NETWORK_TCP;

	switch (n->type) {
	case NETWORK_TCP:
		config_load_servers(n);
		break;
	case NETWORK_PROGRAM:
		n->type_settings.program_location = g_key_file_get_string(kf, "global", "program", NULL);
		break;
	case NETWORK_VIRTUAL:
		n->type_settings.virtual_type = g_key_file_get_string(kf, "global", "virtual", NULL);
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

static struct network_config *find_create_network_config(struct ctrlproxy_config *cfg, const char *name)
{
	GList *gl;
	struct network_config *nc;
	struct tcp_server_config *tc;

	for (gl = cfg->networks; gl; gl = gl->next) {
		GList *gl1;
		nc = gl->data;

		if (g_strcasecmp(nc->name, name) == 0)
			return nc;

		if (nc->type != NETWORK_TCP) 
			continue;

		for (gl1 = nc->type_settings.tcp_servers; gl1; gl1 = gl1->next) {
			char *tmp;
			struct tcp_server_config *sc = gl1->data;

			if (g_strcasecmp(sc->host, name) == 0)
				return nc;

			if (g_strncasecmp(sc->host, name, strlen(sc->host)) != 0)
				continue;

			tmp = g_strdup_printf("%s:%s", sc->host, sc->port);

			if (g_strcasecmp(tmp, name) == 0)
				return nc;

			g_free(tmp);
		}
	}

	nc = network_config_init(cfg);
	nc->name = g_strdup(name);
	nc->autoconnect = FALSE;
	nc->reconnect_interval = DEFAULT_RECONNECT_INTERVAL;
	nc->type = NETWORK_TCP;
	tc = g_new0(struct tcp_server_config, 1);
	tc->host = g_strdup(name);
	if (strchr(tc->host, ':')) {
		tc->port = tc->host+1;
		*tc->port = '\0';
	} else {
		tc->port = g_strdup("6667");
	}

	nc->type_settings.tcp_servers = g_list_append(nc->type_settings.tcp_servers, tc);

	cfg->networks = g_list_append(cfg->networks, nc);

	return nc;
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

struct ctrlproxy_config *load_configuration(const char *dir) 
{
	GKeyFile *kf;
	GError *error = NULL;
	struct ctrlproxy_config *cfg;
	char *file;
	char **autoconnect_list;
	GList *gl;
	gsize size;
	int i;

	file = g_build_filename(dir, "config", NULL);

	cfg = g_new0(struct ctrlproxy_config, 1);
	cfg->config_dir = g_strdup(dir);

	kf = cfg->keyfile = g_key_file_new();

	if (!g_key_file_load_from_file(kf, file, G_KEY_FILE_KEEP_COMMENTS, &error)) {
		log_global(NULL, LOG_ERROR, "Can't parse configuration file '%s': %s", file, error->message);
		g_key_file_free(kf);
		g_free(file);
		g_free(cfg);
		return NULL;
	}

	cfg->autosave = TRUE;
	if (g_key_file_has_key(kf, "global", "autosave", NULL) &&
		!g_key_file_get_boolean(kf, "global", "autosave", NULL))
		cfg->autosave = FALSE;

	cfg->replication = g_key_file_get_string(kf, "global", "replication", NULL);
	cfg->linestack_backend = g_key_file_get_string(kf, "global", "linestack", NULL);

	if (g_key_file_has_key(kf, "global", "report-time", NULL) &&
		!g_key_file_get_boolean(kf, "global", "report-time", NULL))
		cfg->report_time = TRUE;

    if (g_key_file_has_key(kf, "global", "motd-file", NULL))
		cfg->motd_file = g_key_file_get_string(kf, "global", "motd-file", NULL);
    else 
	    cfg->motd_file = g_build_filename(SHAREDIR, "motd", NULL);

	if(!g_file_test(cfg->motd_file, G_FILE_TEST_EXISTS))
		log_global(NULL, LOG_ERROR, "Can't open MOTD file '%s' for reading", cfg->motd_file);

    if (g_key_file_has_key(kf, "admin", "without_privmsg", NULL))
        cfg->admin_noprivmsg = g_key_file_get_boolean(kf, "admin", "without_privmsg", NULL);

	for (gl = cfg->networks; gl; gl = gl->next) {
		struct network_config *nc = gl->data;

		nc->autoconnect = FALSE;
	}

	config_load_networks(cfg);

	size = 0;
	autoconnect_list = g_key_file_get_string_list(kf, "global", "autoconnect", &size, NULL);
		
	for (i = 0; i < size; i++) {
		struct network_config *nc = find_create_network_config(cfg, autoconnect_list[i]);

		g_assert(nc);
		nc->autoconnect = TRUE;
	}

	g_free(file);

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
		g_key_file_free(nc->keyfile);
		g_free(nc);
	}
	g_free(cfg->config_dir);
	g_free(cfg->replication);
	g_free(cfg->linestack_backend);
	g_free(cfg->motd_file);
	g_key_file_free(cfg->keyfile);
	g_free(cfg);
}

