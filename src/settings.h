/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2005 Jelmer Vernooĳ <jelmer@jelmer.uk>

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

#ifndef __CTRLPROXY_SETTINGS_H__
#define __CTRLPROXY_SETTINGS_H__

/**
 * @file
 * @brief Settings
 */

#define DEFAULT_CLIENT_CHARSET NULL

/**
 * Configuration for a particular channel
 */
struct channel_config {
	char *name;
	char *key;
	gboolean autojoin;
};

/**
 * TCP/IP server configuration
 */
struct tcp_server_config {
	/** IP Address or host. */
	char *host;

	/** Port number or service name. */
	char *port;

	/** Address to bind to (or NULL for default) */
	char *bind_address;

	/** Whether SSL should be used. */
	gboolean ssl;

	/** Password to send to remote host. */
	char *password;
};

/**
 * Network configuration.
 */
struct network_config
{
	GKeyFile *keyfile;
	char *groupname; /* name of the keyfile group in keyfile */
	char *filename; /* filename of the keyfile, if different from global */
	/** Network name */
	char *name;
	char *nick;
	char *fullname;
	char *username;
	char *password;

	/** Whether this network was created implicitly. */
	int implicit:1;

	/** Whether to automatically connect to this network on startup. */
	int autoconnect:1;

	/** Whether the first NICK sent by clients to this network should be ignored. */
	int ignore_first_nick:1;

	/** Disable reply caching for this network. */
	int disable_cache:1;

	/** For flood protection */
	int queue_speed;

	/** After how much seconds to attempt to reconnect. */
	guint reconnect_interval;

	/** Channels that should be joined. */
	GList *channels;

	enum {
		NETWORK_TCP,
		NETWORK_PROGRAM,
		NETWORK_VIRTUAL,
		NETWORK_IOCHANNEL
	} type;

	union {
		char *program_location;
		char *virtual_name;
		struct {
			char *default_bind_address;
			GList *servers;
		} tcp;
	} type_settings;

	/** Commands to send on connect. */
	char **autocmd;

	struct ctrlproxy_config *global;
};

/**
 * Log file configuration. Contains substitution variable for
 * the path of the log file and for the format used for various events
 * when logging.
 */
struct log_file_config {
	gboolean logbasedir_is_default;
	const char *logbasedir;
	const char *logfilename;
	gboolean is_irssi;
	const char *nickchange;
	const char *join;
	const char *part;
	const char *topic;
	const char *notopic;
	const char *msg;
	const char *action;
	const char *kick;
	const char *mode;
	const char *quit;
	const char *notice;
};

/**
 * Allowed user/password combination for SOCKS.
 */
struct allow_rule {
	char *username;
	char *password;
};

/**
 * Configuration for a single listener
 */
struct listener_config {
	/** Whether SSL is required */
	gboolean ssl;
	gpointer ssl_credentials;
	/* SSL certificate file */
	char *certfile;
	/* SSL key file */
	char *keyfile;

	/** Password */
	char *password;

	/** Address to bind to. Optional. */
	char *address;

	/** Port to listen on */
	char *port;

	/** Default network this listener is connected to. Can be NULL. */
	char *network;
	GList *allow_rules;

	/** Whether this is the "default" listener, stored in ~/.ctrlproxy/config */
	gboolean is_default;
};

/**
 * Configuration
 */
struct ctrlproxy_config {
	char *config_dir;
	GList *networks;
	gboolean autosave;
	char *motd_file;
	char *network_socket;
	char *linestack_dir;
	char *admin_socket;
	char *password;

	char *default_nick;
	char *default_username;
	char *default_realname;

	/** Whether to create networks that don't exist yet. */
	gboolean create_implicit;

	/** Name of replication backend, or NULL for default. */
	char *replication;

	char *client_charset;
	gboolean admin_log;
	char *admin_user;
	enum {
		REPORT_TIME_ALWAYS,
		REPORT_TIME_NEVER,
		REPORT_TIME_REPLICATION
	} report_time;
	int report_time_offset;
	struct cache_settings cache;
	GKeyFile *keyfile;
	GList *listeners;
	/** Default listener */
	struct listener_config *default_listener;
	gboolean auto_listener;
	int listener_autoport;
	gboolean learn_nickserv;
	gboolean learn_network_name;
	/**
	 * Auto-away configuration.
	 */
	struct auto_away_config {
		/** Idle time after which to set /AWAY, in seconds. */
		int max_idle_time;
		gint client_limit;

		gboolean enabled;

		/** Away message to set. */
		char *message;

		/** Nick to change to when auto away. */
		char *nick;
	} auto_away;

	struct log_file_config *log_file;
};

/* config.c */
G_GNUC_WARN_UNUSED_RESULT G_MODULE_EXPORT struct network_config *network_config_init(struct ctrlproxy_config *cfg);
G_MODULE_EXPORT void save_configuration(struct ctrlproxy_config *cfg, const char *name);
G_GNUC_WARN_UNUSED_RESULT G_MODULE_EXPORT struct ctrlproxy_config *load_configuration(const char *dir, gboolean from_source);
G_GNUC_WARN_UNUSED_RESULT G_MODULE_EXPORT struct ctrlproxy_config *init_configuration(void);
G_MODULE_EXPORT void free_config(struct ctrlproxy_config *);
G_MODULE_EXPORT void setup_configdir(const char *dir);
G_MODULE_EXPORT gboolean g_key_file_save_to_file(GKeyFile *kf, const gchar *file, GError **error);
G_MODULE_EXPORT void config_del_network(struct ctrlproxy_config *cfg, const char *name);

#endif /* __CTRLPROXY_SETTINGS_H__ */
