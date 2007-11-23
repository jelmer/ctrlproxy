/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2007 Jelmer Vernooij <jelmer@nl.linux.org>

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
#include "ssl.h"

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#include <netdb.h>
#include <sys/socket.h>
#include <glib/gstdio.h>

#define DEFAULT_ADMIN_PORT 6680
#define DEFAULT_SOCKS_PORT 1080

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

		if (g_key_file_has_key(kf, name, "ssl", NULL) || ts->ssl)
			g_key_file_set_boolean(kf, name, "ssl", ts->ssl);

		if (ts->password)
			g_key_file_set_string(kf, name, "password", ts->password);
		else
			g_key_file_remove_key(kf, name, "password", NULL);

		if (ts->bind_address) {
			char *tmp;
			if (ts->bind_port) 
				tmp = g_strdup_printf("%s:%s", 
				                      ts->bind_address,
						      ts->bind_port);
			else
				tmp = g_strdup(ts->bind_address);

			g_key_file_set_string(kf, name, "bind", tmp);

			g_free(tmp);
		} else 
			g_key_file_remove_key(kf, name, "bind", NULL);

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
	if (n->queue_speed)
		g_key_file_set_integer(kf, "global", "queue-speed", n->queue_speed);
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

static void config_save_listeners(struct ctrlproxy_config *cfg, const char *path)
{
	GList *gl;
	char *filename;
	GKeyFile *kf; 
	GError *error = NULL;
	gboolean empty = TRUE;
	char *default_password;

	default_password = g_key_file_get_string(cfg->keyfile, "listener", "password", NULL);

	if (cfg->auto_listener) {
		g_key_file_set_boolean(cfg->keyfile, "listener", "auto", cfg->auto_listener);
		g_key_file_set_integer(cfg->keyfile, "listener", "autoport", cfg->listener_autoport);
	}

	filename = g_build_filename(path, "listener", NULL);

	kf = g_key_file_new();

	for (gl = cfg->listeners; gl; gl = gl->next) {
		struct listener_config *l = gl->data;

		if (l->is_default) {
			g_key_file_set_string(cfg->keyfile, "global", "port", l->port);
			if (l->address != NULL)
				g_key_file_set_string(cfg->keyfile, "global", "bind", l->address);
			if (l->password != NULL)
				g_key_file_set_string(cfg->keyfile, "global", "password", l->password);

			if (g_key_file_has_key(cfg->keyfile, "global", "ssl", NULL) || l->ssl)
				g_key_file_set_boolean(cfg->keyfile, "global", "ssl", l->ssl);

			if (l->network != NULL)
				g_key_file_set_string(cfg->keyfile, "global", "default-network",
								  l->network);
		} else {
			char *tmp;
			empty = FALSE;
			if (!l->address) 
				tmp = g_strdup(l->port);
			else
				tmp = g_strdup_printf("%s:%s", l->address, l->port);

			if (l->password != NULL && 
				!(default_password != NULL && strcmp(l->password, default_password) == 0)) 
				g_key_file_set_string(kf, tmp, "password", l->password);

			if (l->network != NULL) {
				g_key_file_set_string(kf, tmp, "network", l->network);
			}

			g_key_file_set_boolean(kf, tmp, "ssl", l->ssl);
		
			g_free(tmp);
		}
	}

	if (empty) {
		unlink(filename);
	} else { 
		if (!g_key_file_save_to_file(kf, filename, &error)) {
			log_global(LOG_WARNING, "Unable to save to \"%s\": %s", filename, error->message);
		}
	}
	
	g_free(filename);
}

static void config_save_networks(const char *config_dir, GList *networks)
{
	char *networksdir = g_build_filename(config_dir, "networks", NULL);
	GList *gl;

	if (!g_file_test(networksdir, G_FILE_TEST_IS_DIR)) {
		if (g_mkdir(networksdir, 0700) != 0) {
			log_global(LOG_ERROR, "Can't create networks directory '%s': %s", networksdir, strerror(errno));
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
			log_global(LOG_ERROR, "Unable to open configuration directory '%s'\n", configuration_dir);
			return;
		}
	}

	if (!cfg->keyfile)
		cfg->keyfile = g_key_file_new();

	g_key_file_set_boolean(cfg->keyfile, "global", "autosave", cfg->autosave);
	if (cfg->admin_user != NULL)
		g_key_file_set_string(cfg->keyfile, "global", "admin-user", cfg->admin_user);

	if (g_key_file_has_key(cfg->keyfile, "global", "admin-log", NULL) ||
		!cfg->admin_log)
		g_key_file_set_boolean(cfg->keyfile, "global", "admin-log", cfg->admin_log);

	if (g_key_file_has_key(cfg->keyfile, "global", "max_who_age", NULL) ||
		cfg->max_who_age != 0)
		g_key_file_set_integer(cfg->keyfile, "global", "max_who_age", cfg->max_who_age);

	if (g_key_file_has_key(cfg->keyfile, "global", "learn-nickserv", NULL) ||
		!cfg->learn_nickserv)
		g_key_file_set_boolean(cfg->keyfile, "global", "learn-nickserv", cfg->learn_nickserv);

	if (g_key_file_has_key(cfg->keyfile, "global", "learn-network-name", NULL) ||
		!cfg->learn_network_name)
		g_key_file_set_boolean(cfg->keyfile, "global", "learn-network-name", cfg->learn_network_name);

	if (cfg->client_charset != NULL)
		g_key_file_set_string(cfg->keyfile, "global", "client-charset", cfg->client_charset);
	if (cfg->replication)
		g_key_file_set_string(cfg->keyfile, "global", "replication", cfg->replication);
	if (cfg->linestack_backend) 
		g_key_file_set_string(cfg->keyfile, "global", "linestack", cfg->linestack_backend);
	if (cfg->motd_file != NULL)
		g_key_file_set_string(cfg->keyfile, "global", "motd-file", cfg->motd_file);

	g_key_file_set_boolean(cfg->keyfile, "global", "report-time", cfg->report_time);

	config_save_networks(configuration_dir, cfg->networks);

	config_save_listeners(cfg, configuration_dir);

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
		s->port = g_strdup(tmp != NULL?tmp:DEFAULT_IRC_PORT);
		s->bind_address = g_key_file_get_string(n->keyfile, servers[i], "bind", NULL);
		if (s->bind_address && (tmp = strchr(s->bind_address, ':'))) {
			*tmp = '\0';
			s->bind_port = tmp+1;
		}

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
		log_global(LOG_ERROR, "Can't parse configuration file '%s': %s", filename, error->message);
		g_key_file_free(kf);
		return NULL;
	}	

	n = network_config_init(cfg);
	n->keyfile = kf;

	g_free(filename);

	if (g_key_file_has_key(kf, "global", "fullname", NULL)) {
		g_free(n->fullname);
		n->fullname = g_key_file_get_string(kf, "global", "fullname", NULL);
		if (!strcmp(n->fullname, "") || n->fullname[0] == ' ')
			log_global(LOG_WARNING, "Invalid fullname `%s' set for network `%s'", n->fullname, n->name);
	}

	if (g_key_file_has_key(kf, "global", "nick", NULL)) {
		g_free(n->nick);
		n->nick = g_key_file_get_string(kf, "global", "nick", NULL);
		if (!strcmp(n->nick, "") || n->nick[0] == ' ')
			log_global(LOG_WARNING, "Invalid nick name `%s' set for `%s'", n->nick, n->name);
	}

	if (g_key_file_has_key(kf, "global", "reconnect-interval", NULL)) {
		n->reconnect_interval = g_key_file_get_integer(kf, "global", "reconnect-interval", NULL);
	}

	if (g_key_file_has_key(kf, "global", "queue-speed", NULL)) {
		n->queue_speed = g_key_file_get_integer(kf, "global", "queue-speed", NULL);
	}

	if (g_key_file_has_key(kf, "global", "username", NULL)) {
		g_free(n->username);
		n->username = g_key_file_get_string(kf, "global", "username", NULL);
		if (!strcmp(n->username, "") || n->username[0] == ' ')
			log_global(LOG_WARNING, "Invalid username `%s' set for network `%s'", n->username, n->name);
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
	case NETWORK_IOCHANNEL:
		/* Don't store */
		break;
	}

	groups = g_key_file_get_groups(kf, &size);
	for (i = 0; i < size; i++) {
		if (!g_ascii_isalpha(groups[i][0]))
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
		tc->port = g_strdup(DEFAULT_IRC_PORT);
	}

	nc->type_settings.tcp_servers = g_list_append(nc->type_settings.tcp_servers, tc);

	cfg->networks = g_list_append(cfg->networks, nc);

	return nc;
}

static void config_load_listeners_socks(struct ctrlproxy_config *cfg)
{
	char **allows;
	gsize size, i;
	GKeyFile *kf = cfg->keyfile;
	struct listener_config *l;

	allows = g_key_file_get_string_list(kf, "socks", "allow", &size, NULL);

	if (allows == NULL)
		return;

	g_key_file_remove_key(kf, "socks", "allow", NULL);

	l = g_new0(struct listener_config, 1);

	if (g_key_file_has_key(kf, "socks", "port", NULL)) 
		l->port = g_key_file_get_string(kf, "socks", "port", NULL);
	else 
		l->port = g_strdup_printf("%d", DEFAULT_SOCKS_PORT);

	/* We can use the socks listener as default listener, if there was 
	 * no default listener specified */
	if (cfg->listeners == NULL ||
		!((struct listener_config *)cfg->listeners->data)->is_default)
		l->is_default = TRUE;

	g_key_file_remove_key(kf, "socks", "port", NULL);

	for (i = 0; i < size; i++) {
		struct allow_rule *r = g_new0(struct allow_rule, 1);
		char **parts = g_strsplit(allows[i], ":", 2);
					
		r->username = parts[0];
		r->password = parts[1];

		g_free(parts);
		l->allow_rules = g_list_append(l->allow_rules, r);
	}

	g_strfreev(allows);

	cfg->listeners = g_list_append(cfg->listeners, l);
}

static void config_load_listeners(struct ctrlproxy_config *cfg)
{
	char *filename = g_build_filename(cfg->config_dir, "listener", NULL);
	int i;
	char **groups;
	gsize size;
	GKeyFile *kf;
	char *default_password;
	GError *error = NULL;

	default_password = g_key_file_get_string(cfg->keyfile, "listener", "password", NULL);
	if (g_key_file_has_key(cfg->keyfile, "listener", "auto", NULL))
		cfg->auto_listener = g_key_file_get_boolean(cfg->keyfile, "listener", "auto", NULL);

	if (g_key_file_has_key(cfg->keyfile, "listener", "autoport", NULL))
		cfg->listener_autoport = g_key_file_get_integer(cfg->keyfile, "listener", "autoport", NULL);

	if (g_key_file_has_key(cfg->keyfile, "global", "port", NULL)) {
		struct listener_config *l = g_new0(struct listener_config, 1);
		l->port = g_key_file_get_string(cfg->keyfile, "global", "port", NULL);
		l->password = g_key_file_get_string(cfg->keyfile, "global", "password", NULL);
		l->address = g_key_file_get_string(cfg->keyfile, "global", "bind", NULL);
		l->ssl = g_key_file_has_key(cfg->keyfile, "global", "ssl", NULL) &&
				 g_key_file_get_boolean(cfg->keyfile, "global", "ssl", NULL);
		l->is_default = TRUE;

		l->network = g_key_file_get_string(cfg->keyfile, "global", "default-network", NULL);

		cfg->listeners = g_list_append(cfg->listeners, l);
	}

	kf = g_key_file_new();

	if (!g_key_file_load_from_file(kf, filename, G_KEY_FILE_KEEP_COMMENTS, &error)) {
		if (error->code != G_FILE_ERROR_NOENT)
			log_global(LOG_ERROR, "Can't parse configuration file '%s': %s", filename, error->message);
		g_free(filename);
		return;
	}
		
	groups = g_key_file_get_groups(kf, &size);

	for (i = 0; i < size; i++)
	{
		struct listener_config *l;
		char *tmp;
		
		l = g_new0(struct listener_config, 1);

		tmp = g_strdup(groups[i]);
		l->port = strrchr(tmp, ':');
		if (l->port != NULL) {
			l->address = tmp;
			*l->port = '\0';
			l->port++;
		} else {
			l->port = tmp;
			l->address = NULL;
		}

		l->password = g_key_file_get_string(kf, groups[i], "password", NULL);
		if (l->password == NULL)
			l->password = default_password;

		if (g_key_file_has_key(kf, groups[i], "ssl", NULL))
			l->ssl = g_key_file_get_boolean(kf, groups[i], "ssl", NULL);

#ifdef HAVE_GNUTLS
		if (l->ssl)
			l->ssl_credentials = ssl_create_server_credentials(cfg, kf, groups[i]);
#endif

		if (g_key_file_has_key(kf, groups[i], "network", NULL))
			l->network = g_key_file_get_string(kf, groups[i], "network", NULL);

		cfg->listeners = g_list_append(cfg->listeners, l);
	}

	g_strfreev(groups);
	g_free(filename);
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
		if (name[0] == '.' || name[strlen(name)-1] == '~')
			continue;
		config_load_network(cfg, networksdir, name);
	}

	g_free(networksdir);

	g_dir_close(dir);
}

#define FETCH_SETTING(data, kf, name) (data)->name = g_key_file_get_string((kf), "log-custom", __STRING(name), NULL)

static void config_load_log(struct ctrlproxy_config *config)
{
	GKeyFile *kf = config->keyfile;
	struct log_file_config *data;
	char *logbasedir;

	if (g_key_file_has_group(kf, "log-custom")) {
		data = g_new0(struct log_file_config, 1);

		FETCH_SETTING(data, kf, nickchange);
		FETCH_SETTING(data, kf, logfilename);
		FETCH_SETTING(data, kf, topic);
		FETCH_SETTING(data, kf, notopic);
		FETCH_SETTING(data, kf, part);
		FETCH_SETTING(data, kf, join);
		FETCH_SETTING(data, kf, msg);
		FETCH_SETTING(data, kf, notice);
		FETCH_SETTING(data, kf, action);
		FETCH_SETTING(data, kf, kick);
		FETCH_SETTING(data, kf, quit);
		FETCH_SETTING(data, kf, mode);

		log_custom_load(data);
	}

	if (g_key_file_has_group(kf, "log-irssi")) {
		data = g_new0(struct log_file_config, 1);

		data->join = "%h:%M -!- %n [%u] has joined %c";
		data->part = "%h:%M -!- %n [%u] has left %c [%m]";
		data->msg = "%h:%M < %n> %m";
		data->notice = "%h:%M < %n> %m";
		data->action = "%h:%M  * %n %m";
		data->mode = "%h:%M -!- mode/%t [%p %c] by %n";
		data->quit = "%h:%M -!- %n [%u] has quit [%m]";
		data->kick = "%h:%M -!- %t has been kicked by %n [%m]";
		data->topic = "%h:%M -!- %n has changed the topic to %t";
		data->notopic = "%h:%M -!- %n has removed the topic";
		data->nickchange = "%h:%M -!- %n is now known as %r";

		if (!g_key_file_has_key(kf, "log-irssi", "logfile", NULL)) {
			logbasedir = g_build_filename(config->config_dir, 
										  "log_irssi", NULL);
		} else {
			logbasedir = g_key_file_get_string(kf, "log-irssi", "logfile", NULL);
		}

		data->logfilename = g_strdup_printf("%s/%%N/%%@", logbasedir);

		g_free(logbasedir);

		log_custom_load(data);
	}
}

static void config_load_auto_away(struct ctrlproxy_config *config)
{
	struct auto_away_config *d;
	GKeyFile *kf = config->keyfile;

	if (g_key_file_has_group(kf, "auto-away")) {
		d = g_new0(struct auto_away_config, 1);
		
		d->message = g_key_file_get_string(kf, "auto-away", "message", NULL);
		d->nick = g_key_file_get_string(kf, "auto-away", "nick", NULL);
		if (g_key_file_has_key(kf, "auto-away", "client_limit", NULL)) {
			d->client_limit = g_key_file_get_integer(kf, "auto-away", "client_limit", NULL);
			if (g_key_file_has_key(kf, "auto-away", "only_noclient", NULL))
				log_global(LOG_WARNING, "auto-away: not using only_noclient because client_limit is set");
		}
		else if (g_key_file_has_key(kf, "auto-away", "only_noclient", NULL)) {
			d->client_limit = g_key_file_get_boolean(kf, "auto-away", "only_noclient", NULL) ? 0 : -1;
			log_global(LOG_WARNING, "auto-away: only_noclient is deprecated, please use client_limit instead");
		}
		else
			d->client_limit = -1;
		if (g_key_file_has_key(kf, "auto-away", "time", NULL))
			d->max_idle_time = g_key_file_get_integer(kf, "auto-away", "time", NULL);
		else
			d->max_idle_time = AUTO_AWAY_DEFAULT_TIME;
	} else if (g_key_file_has_key(kf, "global", "auto-away-enable", NULL) &&
			   g_key_file_get_boolean(kf, "global", "auto-away-enable", NULL)) {
		d = g_new0(struct auto_away_config, 1);
		
		d->message = g_key_file_get_string(kf, "global", "auto-away-message", NULL);
		d->nick = g_key_file_get_string(kf, "global", "auto-away-nick", NULL);
		if (g_key_file_has_key(kf, "global", "auto-away-client-limit", NULL)) {
			d->client_limit = g_key_file_get_integer(kf, "global", "auto-away-client-limit", NULL);
		}
		else
			d->client_limit = -1;
		if (g_key_file_has_key(kf, "global", "auto-away-time", NULL))
			d->max_idle_time = g_key_file_get_integer(kf, "global", "auto-away-time", NULL);
		else
			d->max_idle_time = AUTO_AWAY_DEFAULT_TIME;
	} else {
		return;
	}

	config->auto_away = d;
}

struct ctrlproxy_config *init_configuration(void)
{
	struct ctrlproxy_config *cfg;
	cfg = g_new0(struct ctrlproxy_config, 1);

	return cfg;
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

	cfg = init_configuration();
	cfg->config_dir = g_strdup(dir);
	cfg->network_socket = g_build_filename(cfg->config_dir, "socket", NULL);
	cfg->admin_socket = g_build_filename(cfg->config_dir, "admin", NULL);

	kf = cfg->keyfile = g_key_file_new();

	if (!g_key_file_load_from_file(kf, file, G_KEY_FILE_KEEP_COMMENTS, &error)) {
		log_global(LOG_ERROR, "Can't parse configuration file '%s': %s", file, error->message);
		g_key_file_free(kf);
		g_free(file);
		g_free(cfg);
		return NULL;
	}

	cfg->autosave = TRUE;
	if (g_key_file_has_key(kf, "global", "autosave", NULL) &&
		!g_key_file_get_boolean(kf, "global", "autosave", NULL))
		cfg->autosave = FALSE;

	if (g_key_file_has_key(kf, "global", "max_who_age", NULL))
		cfg->max_who_age = g_key_file_get_integer(kf, "global", "max_who_age", NULL);

	cfg->replication = g_key_file_get_string(kf, "global", "replication", NULL);
	cfg->linestack_backend = g_key_file_get_string(kf, "global", "linestack", NULL);

	if (g_key_file_has_key(kf, "global", "report-time", NULL))
		cfg->report_time = g_key_file_get_boolean(kf, "global", "report-time", NULL);

    if (g_key_file_has_key(kf, "global", "motd-file", NULL))
		cfg->motd_file = g_key_file_get_string(kf, "global", "motd-file", NULL);
    else 
	    cfg->motd_file = g_build_filename(SHAREDIR, "motd", NULL);

    if (g_key_file_has_key(kf, "client", "charset", NULL))
		cfg->client_charset = g_key_file_get_string(kf, "client", "charset", NULL);
	else if (g_key_file_has_key(kf, "global", "client-charset", NULL))
		cfg->client_charset = g_key_file_get_string(kf, "global", "client-charset", NULL);
    else 
	    cfg->client_charset = NULL;

    if (g_key_file_has_key(kf, "global", "learn-nickserv", NULL))
		cfg->learn_nickserv = g_key_file_get_boolean(kf, "global", "learn-nicksev", NULL);
    else 
	    cfg->learn_nickserv = TRUE;

    if (g_key_file_has_key(kf, "global", "learn-network-name", NULL))
		cfg->learn_network_name = g_key_file_get_boolean(kf, "global", "learn-network-name", NULL);
    else 
	    cfg->learn_network_name = TRUE;

	if (!g_file_test(cfg->motd_file, G_FILE_TEST_EXISTS))
		log_global(LOG_ERROR, "Can't open MOTD file '%s' for reading", cfg->motd_file);

    if (g_key_file_has_key(kf, "admin", "without_privmsg", NULL)) {
		if (g_key_file_get_boolean(kf, "admin", "without_privmsg", NULL)) {
			cfg->admin_user = NULL;
		} else {
			cfg->admin_user = g_strdup("ctrlproxy");
		}
		g_key_file_remove_key(kf, "admin", "without_privmsg", NULL);
	}

	if (g_key_file_has_key(kf, "global", "admin-user", NULL)) {
		cfg->admin_user = g_key_file_get_string(kf, "global", "admin-user", NULL);
	}

	cfg->admin_log = TRUE;
    if (g_key_file_has_key(kf, "admin", "log", NULL) && !g_key_file_get_boolean(kf, "admin", "log", NULL))
        cfg->admin_log = FALSE;
	g_key_file_remove_key(kf, "admin", "log", NULL);
    if (g_key_file_has_key(kf, "global", "admin-log", NULL) && !g_key_file_get_boolean(kf, "global", "admin-log", NULL))
        cfg->admin_log = FALSE;

	for (gl = cfg->networks; gl; gl = gl->next) {
		struct network_config *nc = gl->data;

		nc->autoconnect = FALSE;
	}

	config_load_networks(cfg);
	config_load_listeners(cfg);
	config_load_listeners_socks(cfg);
	config_load_log(cfg);
	config_load_auto_away(cfg);

	size = 0;
	autoconnect_list = g_key_file_get_string_list(kf, "global", "autoconnect", &size, NULL);
		
	for (i = 0; i < size; i++) {
		struct network_config *nc = find_create_network_config(cfg, autoconnect_list[i]);

		g_assert(nc);
		nc->autoconnect = TRUE;
	}

	g_strfreev(autoconnect_list);
	g_free(file);

	return cfg;
}

struct network_config *network_config_init(struct ctrlproxy_config *cfg) 
{
	struct network_config *s = g_new0(struct network_config, 1);

	s->autoconnect = FALSE;
	s->nick = g_strdup(g_get_user_name());
	s->username = g_strdup(g_get_user_name());
	g_assert(s->username != NULL && strlen(s->username) > 0);
	s->fullname = g_strdup(g_get_real_name());
	if (s->fullname == NULL || 
		strlen(s->fullname) == 0) {
		g_free(s->fullname);
		s->fullname = g_strdup(s->username);
	}
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
				g_free(tc->bind_address);
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
		case NETWORK_IOCHANNEL:
			/* Nothing to free */
			break;
		}
		cfg->networks = g_list_remove(cfg->networks, nc);
		if (nc->keyfile) g_key_file_free(nc->keyfile);
		g_free(nc);
	}
	g_free(cfg->config_dir);
	g_free(cfg->network_socket);
	g_free(cfg->admin_socket);
	g_free(cfg->replication);
	g_free(cfg->linestack_backend);
	g_free(cfg->motd_file);
	g_free(cfg->admin_user);
	g_key_file_free(cfg->keyfile);
	g_free(cfg);
}

gboolean create_configuration(const char *config_dir)
{
	struct global *global;
	char port[250];
	struct listener_config *l;
	char *pass;

	if (g_file_test(config_dir, G_FILE_TEST_IS_DIR)) {
		fprintf(stderr, "%s already exists\n", config_dir);
		return FALSE;
	}

	if (g_mkdir(config_dir, 0700) != 0) {
		fprintf(stderr, "Can't create config directory '%s': %s\n", config_dir, strerror(errno));
		return FALSE;
	}

	global = load_global(DEFAULT_CONFIG_DIR);	
	if (global == NULL) { 
		fprintf(stderr, "Unable to load default configuration '%s'\n", DEFAULT_CONFIG_DIR);	
		return FALSE;
	}
	global->config->config_dir = g_strdup(config_dir);
	save_configuration(global->config, config_dir);

	snprintf(port, sizeof(port), "%d", DEFAULT_ADMIN_PORT);
	printf("Please specify port the administration interface should listen on.\n"
		   "Prepend with a colon to listen on a specific address.\n"
		   "Example: localhost:6668\n\nPort [%s]: ", port); fflush(stdout);
	if (!fgets(port, sizeof(port), stdin))
		snprintf(port, sizeof(port), "%d", DEFAULT_ADMIN_PORT);

	if (port[strlen(port)-1] == '\n')
		port[strlen(port)-1] = '\0';

	if (strlen(port) == 0) 
		snprintf(port, sizeof(port), "%d", DEFAULT_ADMIN_PORT);

	l = g_new0(struct listener_config, 1);
	pass = getpass("Please specify a password for the administration interface: "); 
	l->port = port;
	if (!strcmp(pass, "")) {
		fprintf(stderr, "Warning: no password specified. Authentication disabled!\n");
	} else {
		l->password = pass;
	}

	global->config->listeners = g_list_append(global->config->listeners, l);

	return TRUE;
}
