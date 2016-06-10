/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2007 Jelmer Vernooij <jelmer@jelmer.uk>

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
#include "keyfile.h"

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#include <strings.h>
#include <netdb.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib/gstdio.h>

#define DEFAULT_ADMIN_PORT 57000
#define DEFAULT_SOCKS_PORT 1080

#define CHANNEL_KEY_FILE_HEADER \
	"; This file contains channel keys.\n" \
	"; It should contain one entry per line, each entry consisting of: \n" \
	"; a channel name, channel key and network name, separated by tabs.\n" \
	";\n\n"

static GList *known_keys = NULL;

static void config_cleanup_networks_dir(struct ctrlproxy_config *cfg);
static void config_save_log(struct log_file_config *data,
							struct ctrlproxy_config *config);
static void config_save_auto_away(struct auto_away_config *d,
								  struct ctrlproxy_config *config);

static const char *builtin_known_keys[] = {
	"autoconnect",
	"autosave",
	"auto-away-enable",
	"auto-away-message",
	"auto-away-nick",
	"auto-away-client-limit",
	"auto-away-time",
	"create-implicit",
	"max-who-age",
	"max_who_age",
	"replication",
	"report-time",
	"report-time-offset",
	"motd-file",
	"default-client-charset",
	"learn-nickserv",
	"learn-network-name",
	"admin-log",
	"admin-user",
	"password",
	"default-username",
	"default-nick",
	"default-fullname",
	"listener-auto",
	"listener-autoport",
	"logging",
	"logfile",
	"logdir",
	"port",
	"bind",
	"ssl",
	"keytab",
	"default-network",
	"log-logfilename",
	"log-format-nickchange",
	"log-format-topic",
	"log-format-notopic",
	"log-format-part",
	"log-format-join",
	"log-format-msg",
	"log-format-notice",
	"log-format-action",
	"log-format-kick",
	"log-format-quit",
	"log-format-mode",
	NULL
};

static gboolean config_known_key(const char *name)
{
	int i;

	if (g_list_find_custom(known_keys, name, (GCompareFunc)strcmp) != NULL)
		return TRUE;

	for (i = 0; builtin_known_keys[i]; i++)
		if (!strcmp(builtin_known_keys[i], name))
			return TRUE;

	return FALSE;
}

void config_register_known_key(char *name)
{
	if (config_known_key(name))
		return;
	known_keys = g_list_insert_sorted(known_keys, g_strdup(name), (GCompareFunc)strcmp);
}



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

static void config_save_tcp_servers(struct network_config *n, GKeyFile *kf, const char *groupname)
{
	GList *gl;
	int i = 0;
	gchar **values = g_new0(gchar *, g_list_length(n->type_settings.tcp.servers)+1);

	for (gl = n->type_settings.tcp.servers; gl; gl = gl->next) {
		struct tcp_server_config *ts = gl->data;
		char *name = irc_create_url(ts->host, ts->port, ts->ssl);

		values[i] = name;

		if (ts->password)
			g_key_file_set_string(kf, name, "password", ts->password);
		else
			g_key_file_remove_key(kf, name, "password", NULL);

		if (ts->bind_address) {
			g_key_file_set_string(kf, name, "bind", ts->bind_address);
		} else
			g_key_file_remove_key(kf, name, "bind", NULL);

		i++;
	}

	g_key_file_set_string_list(kf, groupname, "servers", (const gchar **)values, i);

	g_strfreev(values);
}

static void config_save_network(struct ctrlproxy_config *cfg,
								const char *dir, struct network_config *n, GList **channel_keys)
{
	GList *gl;
	GKeyFile *kf;
	char **autojoin_list;
	int autojoin_list_count;

	if (n->keyfile == NULL) {
		n->keyfile = cfg->keyfile;
		n->groupname = g_strdup(n->name);
	}

	kf = n->keyfile;

	if (n->fullname != NULL)
		g_key_file_set_string(kf, n->groupname, "fullname", n->fullname);
	else
		g_key_file_remove_key(kf, n->groupname, "fullname", NULL);
	if (n->nick != NULL)
		g_key_file_set_string(kf, n->groupname, "nick", n->nick);
	else
		g_key_file_remove_key(kf, n->groupname, "nick", NULL);
	if (n->username != NULL)
		g_key_file_set_string(kf, n->groupname, "username", n->username);
	else
		g_key_file_remove_key(kf, n->groupname, "username", NULL);
	if (n->autocmd != NULL) {
		g_key_file_set_string_list(kf, n->groupname, "autocmd", (const char *const*)n->autocmd,
							   g_strv_length(n->autocmd));
	} else {
		g_key_file_remove_key(kf, n->groupname, "autocmd", NULL);
	}

	if (n->queue_speed)
		g_key_file_set_integer(kf, n->groupname, "queue-speed", n->queue_speed);
	if (n->reconnect_interval != -1)
		g_key_file_set_integer(kf, n->groupname, "reconnect-interval", n->reconnect_interval);

	switch(n->type) {
	case NETWORK_VIRTUAL:
		g_key_file_set_string(kf, n->groupname, "virtual", n->type_settings.virtual_name);
		break;
	case NETWORK_PROGRAM:
		g_key_file_set_string(kf, n->groupname, "program", n->type_settings.program_location);
		break;
	case NETWORK_TCP:
		config_save_tcp_servers(n, kf, n->groupname);
		if (n->type_settings.tcp.default_bind_address != NULL)
			g_key_file_set_string(kf, n->groupname, "bind",
								  n->type_settings.tcp.default_bind_address);
		else
			g_key_file_remove_key(kf, n->groupname, "bind", NULL);
		break;
	default:break;
	}

	autojoin_list = g_new0(char *, g_list_length(n->channels));
	autojoin_list_count = 0;

	for (gl = n->channels; gl; gl = gl->next) {
		struct channel_config *c = gl->data;
		struct keyfile_entry *key;

		if (c->key) {
			key = g_new0(struct keyfile_entry, 1);
			key->network = n->name;
			key->nick = c->name;
			key->pass = c->key;

			*channel_keys = g_list_append(*channel_keys, key);
		}

		if (c->autojoin) {
			autojoin_list[autojoin_list_count] = c->name;
			autojoin_list_count++;
		}

		g_key_file_remove_group(kf, c->name, NULL);
	}

	if (autojoin_list == NULL)
		g_key_file_remove_key(kf, n->groupname, "autojoin", NULL);
	else
		g_key_file_set_string_list(kf, n->groupname, "autojoin", (const gchar **)autojoin_list,
							   autojoin_list_count);

	g_free(autojoin_list);

	if (n->filename != NULL) {
		g_key_file_save_to_file(kf, n->filename, NULL);
	}
}

static void config_save_listeners(struct ctrlproxy_config *cfg, const char *path)
{
	GList *gl;
	char *filename;
	GKeyFile *kf;
	GError *error = NULL;
	gboolean empty = TRUE;

	if (cfg->password != NULL)
		g_key_file_set_string(cfg->keyfile, "global", "password", cfg->password);

	if (cfg->auto_listener) {
		g_key_file_set_boolean(cfg->keyfile, "global", "listener-auto", cfg->auto_listener);
		g_key_file_set_integer(cfg->keyfile, "global", "listener-autoport", cfg->listener_autoport);
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
			cfg->default_listener = l;
		} else {
			char *tmp;
			empty = FALSE;
			if (!l->address)
				tmp = g_strdup(l->port);
			else
				tmp = g_strdup_printf("%s:%s", l->address, l->port);

			if (l->password != NULL)
				g_key_file_set_string(kf, tmp, "password", l->password);

			if (l->network != NULL) {
				g_key_file_set_string(kf, tmp, "network", l->network);
			}

			g_key_file_set_boolean(kf, tmp, "ssl", l->ssl);

			if (l->ssl) {
				if (l->certfile != NULL) {
					g_key_file_set_string(kf, tmp, "certfile", l->certfile);
				}
				if (l->keyfile != NULL) {
					g_key_file_set_string(kf, tmp, "keyfile", l->keyfile);
				}
			}


			g_free(tmp);
		}
	}

	if (empty) {
		unlink(filename);
	} else {
		if (!g_key_file_save_to_file(kf, filename, &error)) {
			log_global(LOG_WARNING, "Unable to save to \"%s\": %s", filename, error->message);
			g_error_free(error);
		}
	}

	g_free(filename);
}

static void config_save_networks(struct ctrlproxy_config *cfg, const char *config_dir, GList *networks)
{
	char *networksdir = g_build_filename(config_dir, "networks", NULL);
	GList *gl;
	GList *channel_keys = NULL;

	for (gl = networks; gl; gl = gl->next) {
		struct network_config *n = gl->data;
		if (n->filename != NULL && !g_file_test(networksdir, G_FILE_TEST_IS_DIR)) {
			if (g_mkdir(networksdir, 0700) != 0) {
				log_global(LOG_ERROR, "Can't create networks directory '%s': %s", networksdir, strerror(errno));
				return;
			}
		}


		if (!n->implicit)
			config_save_network(cfg, networksdir, n, &channel_keys);
	}

	config_cleanup_networks_dir(cfg);

	if (channel_keys != NULL) {
		char *filename = g_build_filename(cfg->config_dir, "keys",
									  NULL);
		keyfile_write_file(channel_keys, CHANNEL_KEY_FILE_HEADER, filename);
		g_free(filename);

		while (channel_keys) {
			g_free(channel_keys->data);
			channel_keys = channel_keys->next;
		}

		g_list_free(channel_keys);
	}

	g_free(networksdir);
}

/**
 * Save configuration to a configuration directory.
 *
 * @param cfg The configuration to save.
 * @param configuration_dir Directory to save to.
 */
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

	if (g_key_file_has_key(cfg->keyfile, "global", "max_who_age", NULL)) {
		g_key_file_set_integer(cfg->keyfile, "global", "max_who_age", cfg->cache.max_who_age);
	} else if (g_key_file_has_key(cfg->keyfile, "global", "max-who-age", NULL) ||
			 cfg->cache.max_who_age != 0) {
		g_key_file_set_integer(cfg->keyfile, "global", "max-who-age", cfg->cache.max_who_age);
	}

	if (g_key_file_has_key(cfg->keyfile, "global", "learn-nickserv", NULL) ||
		!cfg->learn_nickserv)
		g_key_file_set_boolean(cfg->keyfile, "global", "learn-nickserv", cfg->learn_nickserv);

	if (g_key_file_has_key(cfg->keyfile, "global", "learn-network-name", NULL) ||
		!cfg->learn_network_name)
		g_key_file_set_boolean(cfg->keyfile, "global", "learn-network-name", cfg->learn_network_name);

	if (g_key_file_has_key(cfg->keyfile, "global", "create-implicit", NULL) || !cfg->create_implicit)
		g_key_file_set_boolean(cfg->keyfile, "global", "create-implicit", cfg->create_implicit);

	if (cfg->client_charset != NULL)
		g_key_file_set_string(cfg->keyfile, "global", "default-client-charset", cfg->client_charset);

	if (cfg->replication)
		g_key_file_set_string(cfg->keyfile, "global", "replication", cfg->replication);
	if (cfg->motd_file != NULL)
		g_key_file_set_string(cfg->keyfile, "global", "motd-file", cfg->motd_file);

	switch (cfg->report_time) {
	case REPORT_TIME_ALWAYS:
		g_key_file_set_string(cfg->keyfile, "global", "report-time",
							  "always");
		break;
	case REPORT_TIME_NEVER:
		g_key_file_set_string(cfg->keyfile, "global", "report-time",
							  "never");
		break;
	case REPORT_TIME_REPLICATION:
		g_key_file_set_string(cfg->keyfile, "global", "report-time",
							  "replication");
		break;
	}

	if (cfg->default_realname != NULL)
		g_key_file_set_string(cfg->keyfile, "global", "default-fullname", cfg->default_realname);
	else
		g_key_file_remove_key(cfg->keyfile, "global", "default-fullname", NULL);
	if (cfg->default_nick != NULL)
		g_key_file_set_string(cfg->keyfile, "global", "default-nick", cfg->default_nick);
	else
		g_key_file_remove_key(cfg->keyfile, "global", "default-nick", NULL);
	if (cfg->default_username != NULL)
		g_key_file_set_string(cfg->keyfile, "global", "default-username", cfg->default_username);
	else
		g_key_file_remove_key(cfg->keyfile, "global", "default-username", NULL);

	if (cfg->report_time_offset != 0 ||
		g_key_file_has_key(cfg->keyfile, "global", "report-time-offset", NULL))
		g_key_file_set_integer(cfg->keyfile, "global", "report-time-offset", cfg->report_time_offset);

	config_save_networks(cfg, configuration_dir, cfg->networks);

	config_save_listeners(cfg, configuration_dir);

	config_save_log(cfg->log_file, cfg);

	config_save_auto_away(&cfg->auto_away, cfg);

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

	servers = g_key_file_get_string_list(n->keyfile, n->groupname, "servers", &size, NULL);

	if (!servers)
		return;

	for (i = 0; i < size; i++) {
		struct tcp_server_config *s = g_new0(struct tcp_server_config, 1);

		irc_parse_url(servers[i], &s->host, &s->port, &s->ssl);

		s->password = g_key_file_get_string(n->keyfile, servers[i], "password", NULL);
		if (g_key_file_has_key(n->keyfile, servers[i], "ssl", NULL))
			s->ssl = g_key_file_get_boolean(n->keyfile, servers[i], "ssl", NULL);

		s->bind_address = g_key_file_get_string(n->keyfile, servers[i], "bind", NULL);

		n->type_settings.tcp.servers = g_list_append(n->type_settings.tcp.servers, s);

		g_key_file_remove_group(n->keyfile, servers[i], NULL);
	}

	g_strfreev(servers);
}

static struct channel_config *config_find_add_channel(struct network_config *nc, const char *name)
{
	GList *gl;
	struct channel_config *cc;

	for (gl = nc->channels; gl; gl = gl->next) {
		cc = gl->data;
		if (!strcasecmp(cc->name, name))
			return cc;

	}

	cc = g_new0(struct channel_config, 1);
	cc->name = g_strdup(name);

	nc->channels = g_list_append(nc->channels, cc);

	return cc;
}

static struct network_config *config_load_network_keyfile_group(struct ctrlproxy_config *cfg,
																const char *networkname,
																GKeyFile *kf,
																const char *groupname, GList *channel_keys)
{
	struct network_config *n;
	GList *gl;
	gsize size;

	n = network_config_init(cfg);
	n->keyfile = kf;
	n->groupname = g_strdup(groupname);

	if (g_key_file_has_key(kf, groupname, "fullname", NULL)) {
		g_free(n->fullname);
		n->fullname = g_key_file_get_string(kf, groupname, "fullname", NULL);
		if (!strcmp(n->fullname, "") || n->fullname[0] == ' ')
			log_global(LOG_WARNING, "Invalid fullname `%s' set for network `%s'", n->fullname, n->name);
	}

	if (g_key_file_has_key(kf, groupname, "nick", NULL)) {
		g_free(n->nick);
		n->nick = g_key_file_get_string(kf, groupname, "nick", NULL);
		if (!strcmp(n->nick, "") || n->nick[0] == ' ')
			log_global(LOG_WARNING, "Invalid nick name `%s' set for `%s'", n->nick, n->name);
	}

	if (g_key_file_has_key(kf, groupname, "reconnect-interval", NULL)) {
		n->reconnect_interval = g_key_file_get_integer(kf, groupname, "reconnect-interval", NULL);
	}

	if (g_key_file_has_key(kf, groupname, "queue-speed", NULL)) {
		n->queue_speed = g_key_file_get_integer(kf, groupname, "queue-speed", NULL);
	}

	if (g_key_file_has_key(kf, groupname, "username", NULL)) {
		g_free(n->username);
		n->username = g_key_file_get_string(kf, groupname, "username", NULL);
		if (!strcmp(n->username, "") || n->username[0] == ' ')
			log_global(LOG_WARNING, "Invalid username `%s' set for network `%s'", n->username, n->name);
	}

	if (g_key_file_has_key(kf, groupname, "ignore_first_nick", NULL)) {
		n->ignore_first_nick = g_key_file_get_boolean(kf, groupname, "ignore_first_nick", NULL);
	}

	if (g_key_file_has_key(kf, groupname, "password", NULL)) {
		g_free(n->password);
		n->password = g_key_file_get_string(kf, groupname, "password", NULL);
	}

	if (g_key_file_has_key(kf, groupname, "autocmd", NULL)) {
		g_strfreev(n->autocmd);
		n->autocmd = g_key_file_get_string_list(n->keyfile, groupname, "autocmd", &size, NULL);
	}

	n->name = g_strdup(networkname);

	if (g_key_file_has_key(kf, groupname, "program", NULL))
		n->type = NETWORK_PROGRAM;
	else if (g_key_file_has_key(kf, groupname, "virtual", NULL))
		n->type = NETWORK_VIRTUAL;
	else
		n->type = NETWORK_TCP;

	switch (n->type) {
	case NETWORK_TCP:
		n->type_settings.tcp.default_bind_address = g_key_file_get_string(kf, groupname, "bind", NULL);
		config_load_servers(n);
		break;
	case NETWORK_PROGRAM:
		n->type_settings.program_location = g_key_file_get_string(kf, groupname, "program", NULL);
		break;
	case NETWORK_VIRTUAL:
		n->type_settings.virtual_name = g_key_file_get_string(kf, groupname, "virtual", NULL);
		break;
	case NETWORK_IOCHANNEL:
		/* Don't store */
		break;
	}

	for (gl = channel_keys; gl; gl = gl->next) {
		struct keyfile_entry *ke = gl->data;

		if (!strcasecmp(ke->network, n->name)) {
			struct channel_config *cc = config_find_add_channel(n, n->nick);

			g_free(cc->key);
			cc->key = g_strdup(n->password);
		}
	}

	if (g_key_file_has_key(n->keyfile, groupname, "autojoin", NULL)) {
		char **autojoin_channels;
		gsize i;
		autojoin_channels = g_key_file_get_string_list(n->keyfile, groupname, "autojoin", &size, NULL);
		for (i = 0; i < size; i++) {
			struct channel_config *cc = config_find_add_channel(n, autojoin_channels[i]);

			cc->autojoin = TRUE;
		}

		g_strfreev(autojoin_channels);
	}

	return n;
}

static struct network_config *config_load_network_file(struct ctrlproxy_config *cfg, const char *dirname,
												  const char *name, GList *channel_keys)
{
	GKeyFile *kf;
	char *filename;
	int i;
	char **groups;
	GError *error = NULL;
	gsize size;
	struct network_config *n;

	kf = g_key_file_new();

	filename = g_build_filename(dirname, name, NULL);

	if (!g_key_file_load_from_file(kf, filename, G_KEY_FILE_KEEP_COMMENTS, &error)) {
		log_global(LOG_ERROR, "Can't parse configuration file '%s': %s", filename, error->message);
		g_free(filename);
		g_key_file_free(kf);
		g_error_free(error);
		return NULL;
	}

	n = config_load_network_keyfile_group(cfg, name, kf, "global", channel_keys);
	n->filename = filename;

	groups = g_key_file_get_groups(n->keyfile, &size);
	for (i = 0; i < size; i++) {
		if (!g_ascii_isalpha(groups[i][0]))
			config_load_channel(n, n->keyfile, groups[i]);
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

		if (strcasecmp(nc->name, name) == 0) {
			return nc;
		}

		if (nc->type != NETWORK_TCP) {
			continue;
		}

		for (gl1 = nc->type_settings.tcp.servers; gl1; gl1 = gl1->next) {
			char *tmp;
			struct tcp_server_config *sc = gl1->data;

			if (strcasecmp(sc->host, name) == 0) {
				return nc;
			}

			if (strncasecmp(sc->host, name, strlen(sc->host)) != 0) {
				continue;
			}

			tmp = irc_create_url(sc->host, sc->port, FALSE);

			if (strcasecmp(tmp, name) == 0) {
				return nc;
			}

			g_free(tmp);
		}
	}

	nc = network_config_init(cfg);
	nc->name = g_strdup(name);
	nc->autoconnect = FALSE;
	nc->reconnect_interval = -1;
	nc->type = NETWORK_TCP;
	tc = g_new0(struct tcp_server_config, 1);
	irc_parse_url(name, &tc->host, &tc->port, &tc->ssl);
	nc->type_settings.tcp.servers = g_list_append(nc->type_settings.tcp.servers, tc);

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

	if (allows == NULL) {
		return;
	}

	g_key_file_remove_key(kf, "socks", "allow", NULL);

	l = g_new0(struct listener_config, 1);

	if (g_key_file_has_key(kf, "socks", "port", NULL)) {
		l->port = g_key_file_get_string(kf, "socks", "port", NULL);
	} else {
		l->port = g_strdup_printf("%d", DEFAULT_SOCKS_PORT);
	}

	/* We can use the socks listener as default listener, if there was
	 * no default listener specified */
	if (cfg->listeners == NULL ||
		!((struct listener_config *)cfg->listeners->data)->is_default) {
		l->is_default = TRUE;
	}

	g_key_file_remove_key(kf, "socks", "port", NULL);

	for (i = 0; i < size; i++) {
		struct allow_rule *r = g_new0(struct allow_rule, 1);
		char **parts = g_strsplit(allows[i], ":", 2);

		r->username = parts[0];
		r->password = parts[1];

		g_free(parts);
		l->allow_rules = g_list_append(l->allow_rules, r);
	}

	g_key_file_remove_group(kf, "socks", NULL);

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
	GError *error = NULL;

	if (g_key_file_has_key(cfg->keyfile, "listener", "pasword", NULL)) {
		g_key_file_set_string(cfg->keyfile, "global", "password",
							  g_key_file_get_string(cfg->keyfile, "listener", "password", NULL));
		g_key_file_remove_key(cfg->keyfile, "listener", "password", NULL);
	}
	cfg->password = g_key_file_get_string(cfg->keyfile, "global", "password", NULL);

	if (g_key_file_has_key(cfg->keyfile, "global", "listener-auto", NULL)) {
		cfg->auto_listener = g_key_file_get_boolean(cfg->keyfile, "global", "listener-auto", NULL);
	} else if (g_key_file_has_key(cfg->keyfile, "listener", "auto", NULL)) {
		cfg->auto_listener = g_key_file_get_boolean(cfg->keyfile, "listener", "auto", NULL);
		g_key_file_remove_key(cfg->keyfile, "listener", "auto", NULL);
	}

	if (g_key_file_has_key(cfg->keyfile, "global", "listener-autoport", NULL)) {
		cfg->listener_autoport = g_key_file_get_integer(cfg->keyfile, "global", "listener-autoport", NULL);
	} else if (g_key_file_has_key(cfg->keyfile, "listener", "autoport", NULL)) {
		cfg->listener_autoport = g_key_file_get_integer(cfg->keyfile, "listener", "autoport", NULL);
		g_key_file_remove_key(cfg->keyfile, "listener", "autoport", NULL);
	}

	if (g_key_file_has_key(cfg->keyfile, "global", "port", NULL)) {
		struct listener_config *l = g_new0(struct listener_config, 1);
		l->port = g_key_file_get_string(cfg->keyfile, "global", "port", NULL);
		l->password = g_key_file_get_string(cfg->keyfile, "global", "password", NULL);
		l->address = g_key_file_get_string(cfg->keyfile, "global", "bind", NULL);
		l->ssl = g_key_file_has_key(cfg->keyfile, "global", "ssl", NULL) &&
				 g_key_file_get_boolean(cfg->keyfile, "global", "ssl", NULL);

#ifdef HAVE_GNUTLS
		if (l->ssl) {
			l->ssl_credentials = ssl_create_server_credentials(cfg->config_dir, cfg->keyfile, "global");
			if (g_key_file_has_key(cfg->keyfile, "global", "certfile", NULL)) {
				l->certfile = g_key_file_get_string(cfg->keyfile, "global", "certfile", NULL);
			}
			if (g_key_file_has_key(cfg->keyfile, "global", "keyfile", NULL)) {
				l->keyfile = g_key_file_get_string(cfg->keyfile, "global", "keyfile", NULL);
			}
		}
#endif
		l->is_default = TRUE;

		l->network = g_key_file_get_string(cfg->keyfile, "global", "default-network", NULL);

		cfg->listeners = g_list_append(cfg->listeners, l);
	}

	kf = g_key_file_new();

	if (!g_key_file_load_from_file(kf, filename, G_KEY_FILE_KEEP_COMMENTS, &error)) {
		if (error->code != G_FILE_ERROR_NOENT) {
			log_global(LOG_ERROR, "Can't parse configuration file '%s': %s", filename, error->message);
		}
		g_error_free(error);
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

		if (g_key_file_has_key(kf, groups[i], "ssl", NULL))
			l->ssl = g_key_file_get_boolean(kf, groups[i], "ssl", NULL);

#ifdef HAVE_GNUTLS
		if (l->ssl) {
			l->ssl_credentials = ssl_create_server_credentials(cfg->config_dir, kf, groups[i]);
			if (g_key_file_has_key(kf, groups[i], "certfile", NULL)) {
				l->certfile = g_key_file_get_string(kf, groups[i], "certfile", NULL);
			}
			if (g_key_file_has_key(kf, groups[i], "keyfile", NULL)) {
				l->keyfile = g_key_file_get_string(kf, groups[i], "keyfile", NULL);
			}
		}
#endif

		if (g_key_file_has_key(kf, groups[i], "network", NULL)) {
			l->network = g_key_file_get_string(kf, groups[i], "network", NULL);
		}

		cfg->listeners = g_list_append(cfg->listeners, l);
	}

	g_strfreev(groups);
	g_free(filename);
}


struct network_config *config_find_network(struct ctrlproxy_config *cfg,
										   const char *name)
{
	GList *gl;
	for (gl = cfg->networks; gl; gl = gl->next) {
		struct network_config *nc = gl->data;
		if (!strcasecmp(nc->name, name))
			return nc;
	}
	return NULL;
}

void config_del_network(struct ctrlproxy_config *cfg, const char *name)
{
	cfg->networks = g_list_remove(cfg->networks, config_find_network(cfg, name));
}

#define IS_SPECIAL_FILE(name) (name[0] == '.' || name[strlen(name)-1] == '~')

static void config_cleanup_networks_dir(struct ctrlproxy_config *cfg)
{
	char *networksdir = g_build_filename(cfg->config_dir, "networks", NULL);
	GDir *dir;
	const char *name;

	dir = g_dir_open(networksdir, 0, NULL);
	if (dir == NULL) {
		return;
	}

	while ((name = g_dir_read_name(dir))) {
		char *path;
		if (IS_SPECIAL_FILE(name) || config_find_network(cfg, name)) {
			continue;
		}

		path = g_build_filename(networksdir, name, NULL);
		g_unlink(path);
		g_free(path);
	}

	g_free(networksdir);

	g_dir_close(dir);
}

static void config_load_networks(struct ctrlproxy_config *cfg, GList *channel_keys)
{
	char *networksdir = g_build_filename(cfg->config_dir, "networks", NULL);
	GDir *dir;
	const char *name;
	char **groups;
	gsize size, i;
	struct network_config *n;

	/* Load networks from files */

	dir = g_dir_open(networksdir, 0, NULL);
	if (dir != NULL) {
		while ((name = g_dir_read_name(dir))) {
			if (IS_SPECIAL_FILE(name)) {
				continue;
			}
			n = config_load_network_file(cfg, networksdir, name, channel_keys);
			g_assert(n != NULL);
		}

		g_dir_close(dir);
	}
	g_free(networksdir);


	/* Load other networks in configuration file */
	groups = g_key_file_get_groups(cfg->keyfile, &size);
	for (i = 0; i < size; i++) {
		if (!strcmp(groups[i], "global")) {
			continue;
		}

		if (!strncasecmp(groups[i], "irc://", strlen("irc://")) ||
			!strncasecmp(groups[i], "ircs://", strlen("ircs://"))) {
			continue;
		}

		if (groups[i][0] == '#' || groups[i][0] == '&') {
			continue;
		}

		n = config_load_network_keyfile_group(cfg, groups[i], cfg->keyfile, groups[i], channel_keys);
		g_assert(n != NULL);
	}

	g_strfreev(groups);
}

#define FETCH_SETTING(data, kf, section, prefix, name) (data)->name = g_key_file_get_string((kf), (section), prefix #name, NULL)
#define STORE_SETTING(data, kf, section, prefix, name) g_key_file_set_string((kf), (section), prefix #name, (data)->name)

static void config_save_log(struct log_file_config *data,
							struct ctrlproxy_config *config)
{
	if (data == NULL) {
		g_key_file_set_string(config->keyfile, "global", "logging", "none");
		return;
	}

	if (data->is_irssi) {
		g_key_file_set_string(config->keyfile, "global", "logging", "irssi");
		if (data->logbasedir) {
			if (!data->logbasedir_is_default) {
				g_key_file_set_string(config->keyfile, "global", "logdir", data->logbasedir);
			}
		} else {
			g_key_file_set_string(config->keyfile, "global", "logfile", data->logfilename);
		}
	} else {
		STORE_SETTING(data, config->keyfile, "global", "", logfilename);
		STORE_SETTING(data, config->keyfile, "global", "log-format-", nickchange);
		STORE_SETTING(data, config->keyfile, "global", "log-format-", topic);
		STORE_SETTING(data, config->keyfile, "global", "log-format-", notopic);
		STORE_SETTING(data, config->keyfile, "global", "log-format-", part);
		STORE_SETTING(data, config->keyfile, "global", "log-format-", join);
		STORE_SETTING(data, config->keyfile, "global", "log-format-", msg);
		STORE_SETTING(data, config->keyfile, "global", "log-format-", notice);
		STORE_SETTING(data, config->keyfile, "global", "log-format-", action);
		STORE_SETTING(data, config->keyfile, "global", "log-format-", kick);
		STORE_SETTING(data, config->keyfile, "global", "log-format-", quit);
		STORE_SETTING(data, config->keyfile, "global", "log-format-", mode);
	}
}

static void config_load_log(struct ctrlproxy_config *config)
{
	GKeyFile *kf = config->keyfile;
	struct log_file_config *data;
	char *logging = NULL;

	if (g_key_file_has_key(kf, "global", "logging", NULL)) {
		logging = g_key_file_get_string(kf, "global", "logging", NULL);
	}

	if (g_key_file_has_group(kf, "log-custom")) {
		data = g_new0(struct log_file_config, 1);

		FETCH_SETTING(data, kf, "log-custom", "", nickchange);
		FETCH_SETTING(data, kf, "log-custom", "", logfilename);
		FETCH_SETTING(data, kf, "log-custom", "", topic);
		FETCH_SETTING(data, kf, "log-custom", "", notopic);
		FETCH_SETTING(data, kf, "log-custom", "", part);
		FETCH_SETTING(data, kf, "log-custom", "", join);
		FETCH_SETTING(data, kf, "log-custom", "", msg);
		FETCH_SETTING(data, kf, "log-custom", "", notice);
		FETCH_SETTING(data, kf, "log-custom", "", action);
		FETCH_SETTING(data, kf, "log-custom", "", kick);
		FETCH_SETTING(data, kf, "log-custom", "", quit);
		FETCH_SETTING(data, kf, "log-custom", "", mode);

		g_key_file_remove_group(kf, "log-custom", NULL);
		config->log_file = data;
		log_custom_load(data);
	}

	if (logging != NULL && !strcmp(logging, "custom")) {
		data = g_new0(struct log_file_config, 1);

		FETCH_SETTING(data, kf, "global", "", logfilename);
		FETCH_SETTING(data, kf, "global", "log-format-", nickchange);
		FETCH_SETTING(data, kf, "global", "log-format-", topic);
		FETCH_SETTING(data, kf, "global", "log-format-", notopic);
		FETCH_SETTING(data, kf, "global", "log-format-", part);
		FETCH_SETTING(data, kf, "global", "log-format-", join);
		FETCH_SETTING(data, kf, "global", "log-format-", msg);
		FETCH_SETTING(data, kf, "global", "log-format-", notice);
		FETCH_SETTING(data, kf, "global", "log-format-", action);
		FETCH_SETTING(data, kf, "global", "log-format-", kick);
		FETCH_SETTING(data, kf, "global", "log-format-", quit);
		FETCH_SETTING(data, kf, "global", "log-format-", mode);

		config->log_file = data;
		log_custom_load(data);
	}

	if (g_key_file_has_group(kf, "log-irssi") ||
		(logging != NULL && !strcmp(logging, "irssi"))) {
		data = g_new0(struct log_file_config, 1);
		data->is_irssi = TRUE;

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

		if (g_key_file_has_key(kf, "global", "logfile", NULL)) {
			data->logfilename= g_key_file_get_string(kf, "global", "logfile", NULL);
		} else if (g_key_file_has_key(kf, "global", "logdir", NULL)) {
			data->logbasedir = g_key_file_get_string(kf, "global", "logdir", NULL);
			data->logfilename = g_strdup_printf("%s/%%N/%%@", data->logbasedir);
		} else if (g_key_file_has_key(kf, "log-irssi", "logfile", NULL)) {
			data->logbasedir = g_key_file_get_string(kf, "log-irssi", "logfile", NULL);
			data->logfilename = g_strdup_printf("%s/%%N/%%@", data->logbasedir);
		} else {
			data->logbasedir = g_build_filename(config->config_dir,
										  "log_irssi", NULL);

			data->logbasedir_is_default = TRUE;

			data->logfilename = g_strdup_printf("%s/%%N/%%@", data->logbasedir);
		}
		g_key_file_remove_group(kf, "log-irssi", NULL);

		config->log_file = data;
		log_custom_load(data);
	}

	if (logging != NULL &&
			strcmp(logging, "irssi") != 0 &&
			strcmp(logging, "custom") != 0 &&
			strcmp(logging, "none") != 0) {
		log_global(LOG_WARNING, "Unknown log type `%s'", logging);
	}

	g_free(logging);
}

static void config_save_auto_away(struct auto_away_config *d, struct ctrlproxy_config *config)
{
	GKeyFile *kf = config->keyfile;

	if (g_key_file_has_key(kf, "global", "auto-away-enable", NULL) ||
		d->enabled) {
		g_key_file_set_boolean(kf, "global", "auto-away-enable", d->enabled);
	}

	if (d->message != NULL) {
		g_key_file_set_string(kf, "global", "auto-away-message", d->message);
	}

	if (d->nick != NULL) {
		g_key_file_set_string(kf, "global", "auto-away-nick", d->nick);
	}

	if (d->client_limit != -1) {
		g_key_file_set_integer(kf, "global", "auto-away-client-limit", d->client_limit);
	}

	if (d->max_idle_time != -1) {
		g_key_file_set_integer(kf, "global", "auto-away-time", d->max_idle_time);
	}
}

static void config_load_auto_away(struct auto_away_config *d, GKeyFile *kf)
{
	if (g_key_file_has_group(kf, "auto-away")) {
		d->enabled = TRUE;
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
			d->max_idle_time = -1;

		g_key_file_remove_group(kf, "auto-away", NULL);
	} else {
		if (g_key_file_has_key(kf, "global", "auto-away-enable", NULL))
			d->enabled = g_key_file_get_boolean(kf, "global", "auto-away-enable", NULL);
		d->message = g_key_file_get_string(kf, "global", "auto-away-message", NULL);
		d->nick = g_key_file_get_string(kf, "global", "auto-away-nick", NULL);
		if (g_key_file_has_key(kf, "global", "auto-away-client-limit", NULL)) {
			d->client_limit = g_key_file_get_integer(kf, "global", "auto-away-client-limit", NULL);
		} else
			d->client_limit = -1;
		if (g_key_file_has_key(kf, "global", "auto-away-time", NULL))
			d->max_idle_time = g_key_file_get_integer(kf, "global", "auto-away-time", NULL);
		else
			d->max_idle_time = -1;
	}
}

struct ctrlproxy_config *init_configuration(void)
{
	struct ctrlproxy_config *cfg;
	cfg = g_new0(struct ctrlproxy_config, 1);

	return cfg;
}

struct ctrlproxy_config *load_configuration(const char *dir,
											gboolean from_source)
{
	GKeyFile *kf;
	GError *error = NULL;
	struct ctrlproxy_config *cfg;
	char **keys;
	char *file;
	char **autoconnect_list;
	GList *gl;
	gsize size;
	int i;
	GList *channel_keys = NULL;
	char *keyfile_filename;

	file = g_build_filename(dir, "config", NULL);

	cfg = init_configuration();
	cfg->config_dir = g_strdup(dir);
	cfg->linestack_dir = g_build_filename(dir, "linestack", NULL);
	cfg->network_socket = g_build_filename(cfg->config_dir, "socket", NULL);
	cfg->admin_socket = g_build_filename(cfg->config_dir, "admin", NULL);

	kf = cfg->keyfile = g_key_file_new();

	if (!g_key_file_load_from_file(kf, file, G_KEY_FILE_KEEP_COMMENTS, &error)) {
		log_global(LOG_ERROR, "Can't parse configuration file '%s': %s", file, error->message);
		g_error_free(error);
		g_key_file_free(kf);
		g_free(file);
		g_free(cfg);
		return NULL;
	}

	cfg->autosave = TRUE;
	if (g_key_file_has_key(kf, "global", "autosave", NULL) &&
		!g_key_file_get_boolean(kf, "global", "autosave", NULL)) {
		cfg->autosave = FALSE;
	}

	if (g_key_file_has_key(kf, "global", "keytab", NULL)) {
		char *keytab;
		keytab = g_key_file_get_string(kf, "global", "keytab", NULL);
#ifdef HAVE_GSSKRB5_REGISTER_ACCEPTOR_IDENTITY
		gsskrb5_register_acceptor_identity(keytab);
#endif
		g_free(keytab);
	}

	if (g_key_file_has_key(kf, "global", "max_who_age", NULL)) {
		cfg->cache.max_who_age = g_key_file_get_integer(kf, "global", "max_who_age", NULL);
	} else if (g_key_file_has_key(kf, "global", "max-who-age", NULL)) {
		cfg->cache.max_who_age = g_key_file_get_integer(kf, "global", "max-who-age", NULL);
	}


	cfg->replication = g_key_file_get_string(kf, "global", "replication", NULL);

	if (g_key_file_has_key(kf, "global", "report-time", NULL)) {
		char *setting = g_key_file_get_string(kf, "global", "report-time", NULL);
		if (!strcasecmp(setting, "never") || !strcasecmp(setting, "false")) {
			cfg->report_time = REPORT_TIME_NEVER;
		} else if (!strcasecmp(setting, "always")) {
			cfg->report_time = REPORT_TIME_ALWAYS;
		} else if  (!strcasecmp(setting, "replication") ||
				  !strcasecmp(setting, "true")) {
			cfg->report_time = REPORT_TIME_REPLICATION;
		} else {
			log_global(LOG_WARNING, "Unknown value `%s' for report-time in configuration file", setting);
		}
		g_free(setting);
	}

	cfg->report_time_offset = 0;
	if (g_key_file_has_key(kf, "global", "report-time-offset", NULL)) {
		cfg->report_time_offset = g_key_file_get_integer(kf, "global", "report-time-offset", NULL);
	}

	if (g_key_file_has_key(kf, "global", "motd-file", NULL)) {
		cfg->motd_file = g_key_file_get_string(kf, "global", "motd-file", NULL);
	} else if (from_source) {
		cfg->motd_file = g_strdup("motd");
	} else {
		cfg->motd_file = g_build_filename(SYSCONFDIR, "ctrlproxy", "motd", NULL);
	}

	if (g_key_file_has_key(kf, "client", "charset", NULL)) {
		cfg->client_charset = g_key_file_get_string(kf, "client", "charset", NULL);
		g_key_file_remove_key(cfg->keyfile, "client", "charset", NULL); /* deprecated */
	} else if (g_key_file_has_key(kf, "global", "client-charset", NULL)) {
		cfg->client_charset = g_key_file_get_string(kf, "global", "client-charset", NULL);
		g_key_file_remove_key(cfg->keyfile, "global", "client-charset", NULL); /* deprecated */
	} else if (g_key_file_has_key(kf, "global", "default-client-charset", NULL)) {
		cfg->client_charset = g_key_file_get_string(kf, "global", "default-client-charset", NULL);
	} else {
		cfg->client_charset = NULL;
	}

	if (g_key_file_has_key(kf, "global", "learn-nickserv", NULL)) {
		cfg->learn_nickserv = g_key_file_get_boolean(kf, "global", "learn-nickserv", NULL);
	} else {
		cfg->learn_nickserv = TRUE;
	}

	if (g_key_file_has_key(kf, "global", "learn-network-name", NULL)) {
		cfg->learn_network_name = g_key_file_get_boolean(kf, "global", "learn-network-name", NULL);
	} else {
		cfg->learn_network_name = TRUE;
	}

	if (g_key_file_has_key(kf, "global", "create-implicit", NULL)) {
		cfg->create_implicit = g_key_file_get_boolean(kf, "global", "create-implicit", NULL);
	} else {
		cfg->create_implicit = TRUE;
	}

	if (g_key_file_has_key(kf, "global", "default-username", NULL)) {
		g_free(cfg->default_username);
		cfg->default_username = g_key_file_get_string(kf, "global", "default-username", NULL);
		if (!strcmp(cfg->default_username, "") || cfg->default_username[0] == ' ') {
			log_global(LOG_WARNING, "Invalid default username `%s' set", cfg->default_username);
		}
	}

	if (g_key_file_has_key(kf, "global", "default-fullname", NULL)) {
		g_free(cfg->default_realname);
		cfg->default_realname = g_key_file_get_string(kf, "global", "default-fullname", NULL);
		if (!strcmp(cfg->default_realname, "") || cfg->default_realname[0] == ' ') {
			log_global(LOG_WARNING, "Invalid default fullname `%s' set", cfg->default_realname);
		}
	}

	if (g_key_file_has_key(kf, "global", "default-nick", NULL)) {
		g_free(cfg->default_nick);
		cfg->default_nick = g_key_file_get_string(kf, "global", "default-nick", NULL);
		if (!strcmp(cfg->default_nick, "") || cfg->default_nick[0] == ' ') {
			log_global(LOG_WARNING, "Invalid nick name `%s' set", cfg->default_nick);
		}
	}

	if (!g_file_test(cfg->motd_file, G_FILE_TEST_EXISTS)) {
		log_global(LOG_ERROR, "Can't open MOTD file '%s' for reading", cfg->motd_file);
	}

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
	if (g_key_file_has_key(kf, "admin", "log", NULL) && !g_key_file_get_boolean(kf, "admin", "log", NULL)) {
        cfg->admin_log = FALSE;
	}
	g_key_file_remove_key(kf, "admin", "log", NULL);
	if (g_key_file_has_key(kf, "global", "admin-log", NULL) &&
		!g_key_file_get_boolean(kf, "global", "admin-log", NULL)) {
		cfg->admin_log = FALSE;
	}

	for (gl = cfg->networks; gl; gl = gl->next) {
		struct network_config *nc = gl->data;

		nc->autoconnect = FALSE;
	}

	config_load_listeners(cfg);
	config_load_listeners_socks(cfg);
	config_load_log(cfg);
	config_load_auto_away(&cfg->auto_away, cfg->keyfile);

	keyfile_filename = g_build_filename(cfg->config_dir, "keys", NULL);

	if (g_file_test(keyfile_filename, G_FILE_TEST_EXISTS)) {
		if (!keyfile_read_file(keyfile_filename, ';', &channel_keys)) {
			log_global(LOG_WARNING, "Unable to read keys file");
		}
	}

	g_free(keyfile_filename);

	config_load_networks(cfg, channel_keys);

	/* Check for unknown parameters */
	keys = g_key_file_get_keys(kf, "global", NULL, NULL);
	for (i = 0; keys[i] != NULL; i++) {
		if (!config_known_key(keys[i])) {
			log_global(LOG_WARNING, "Unknown setting `%s' in configuration file", keys[i]);
		}
	}
	g_strfreev(keys);

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

	s->global = cfg;

	s->autoconnect = FALSE;
	s->reconnect_interval = -1;

	if (cfg) {
		cfg->networks = g_list_append(cfg->networks, s);
	}
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
		g_strfreev(nc->autocmd);
		while (nc->channels) {
			struct channel_config *cc = nc->channels->data;
			g_free(cc->name);
			g_free(cc->key);
			nc->channels = g_list_remove(nc->channels, cc);
			g_free(cc);
		}
		switch (nc->type) {
		case NETWORK_TCP:
			while (nc->type_settings.tcp.servers) {
				struct tcp_server_config *tc = nc->type_settings.tcp.servers->data;
				g_free(tc->host);
				g_free(tc->port);
				g_free(tc->bind_address);
				g_free(tc->password);
				nc->type_settings.tcp.servers = g_list_remove(nc->type_settings.tcp.servers, tc);
				g_free(tc);
			}
			g_free(nc->type_settings.tcp.default_bind_address);
			break;
		case NETWORK_VIRTUAL:
			g_free(nc->type_settings.virtual_name);
			break;
		case NETWORK_PROGRAM:
			g_free(nc->type_settings.program_location);
			break;
		case NETWORK_IOCHANNEL:
			/* Nothing to free */
			break;
		}
		cfg->networks = g_list_remove(cfg->networks, nc);
		if (nc->keyfile != NULL && nc->filename != NULL) {
			g_key_file_free(nc->keyfile);
		}
		g_free(nc->filename);
		g_free(nc->groupname);
		g_free(nc);
	}
	g_free(cfg->config_dir);
	g_free(cfg->network_socket);
	g_free(cfg->linestack_dir);
	g_free(cfg->admin_socket);
	g_free(cfg->replication);
	g_free(cfg->motd_file);
	g_free(cfg->admin_user);
	g_key_file_free(cfg->keyfile);
	g_free(cfg);
}

gboolean create_configuration(const char *config_dir, gboolean from_source)
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

	global = load_global(DEFAULT_CONFIG_DIR, from_source);
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
	l->is_default = TRUE;
	if (!strcmp(pass, "")) {
		fprintf(stderr, "Warning: no password specified. Authentication disabled!\n");
	} else {
		l->password = g_strdup(pass);
	}

	global->config->default_listener = l;

	global->config->listeners = g_list_append(global->config->listeners, l);

	save_configuration(global->config, config_dir);

	return TRUE;
}
