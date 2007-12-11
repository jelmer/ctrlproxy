/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2005 Jelmer Vernooij <jelmer@nl.linux.org>

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
	char *host;
	char *port;
	char *bind_address;
	char *bind_port;
	int ssl:1;
	char *password;
};

/**
 * Network configuration.
 */
struct network_config 
{
	GKeyFile *keyfile;
	char *name;
	char *nick;
	char *fullname;
	char *username;
	char *password;
	int autoconnect:1;
	int ignore_first_nick:1;
	int disable_cache:1;
	int queue_speed; /* For flood protection */
	guint reconnect_interval;

	GList *channels;

	enum { 
		NETWORK_TCP, 
		NETWORK_PROGRAM, 
		NETWORK_VIRTUAL,
		NETWORK_IOCHANNEL
	} type;

	union {
		char *virtual_type;
		char *program_location;
		GList *tcp_servers;
	} type_settings; 
};

/**
 * Log file configuration. Contains substitution variable for 
 * the path of the log file and for the format used for various events 
 * when logging.
 */
struct log_file_config {
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
	gboolean ssl;
	gpointer ssl_credentials;
	char *password;
	char *address;
	char *port;
	char *network;
	GList *allow_rules;
	gboolean is_default; /* Whether this is the "default" listener, stored in ~/.ctrlproxy/config */
};

/**
 * Auto-away configuration.
 */
struct auto_away_config {
	int max_idle_time;
	gint client_limit;
	char *message;
	char *nick;
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
	char *admin_socket;
	char *replication;
	char *linestack_backend;
	char *client_charset;
	gboolean admin_log;
	char *admin_user;
	enum { 
		REPORT_TIME_ALWAYS,
		REPORT_TIME_NEVER,
		REPORT_TIME_REPLICATION
	} report_time;
	int report_time_offset;
	int max_who_age;
	GKeyFile *keyfile;
	GList *listeners;
	gboolean auto_listener;
	int listener_autoport;
	gboolean learn_nickserv;
	gboolean learn_network_name;
	struct auto_away_config *auto_away;
	struct log_file_config *log_file;
};

/* config.c */
G_MODULE_EXPORT struct network_config *network_config_init(struct ctrlproxy_config *cfg);
G_MODULE_EXPORT void save_configuration(struct ctrlproxy_config *cfg, const char *name);
G_MODULE_EXPORT struct ctrlproxy_config *load_configuration(const char *dir);
G_MODULE_EXPORT struct ctrlproxy_config *init_configuration(void);
G_MODULE_EXPORT void free_config(struct ctrlproxy_config *);
G_MODULE_EXPORT void setup_configdir(const char *dir);
G_MODULE_EXPORT gboolean g_key_file_save_to_file(GKeyFile *kf, const gchar *file, GError **error);

#endif /* __CTRLPROXY_SETTINGS_H__ */
