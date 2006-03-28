/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2005 Jelmer Vernooij <jelmer@nl.linux.org>

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

#ifndef __CTRLPROXY_SETTINGS_H__
#define __CTRLPROXY_SETTINGS_H__

struct channel_config {
	char *name;
	char *key;
	int autojoin:1;
};

struct tcp_server_config {
	char *name;
	char *host;
	char *port;
	int ssl:1;
	char *password;
};

struct network_config 
{
	char *name;
	char *nick;
	char *fullname;
	char *username;
	char *password;
	int autoconnect:1;
	int ignore_first_nick:1;
	int disable_cache:1;
	guint reconnect_interval;

	GList *channels;

	enum { 
		NETWORK_TCP, 
		NETWORK_PROGRAM, 
		NETWORK_VIRTUAL 
	} type;

	union {
		char *virtual_type;
		char *program_location;
		GList *tcp_servers;
	} type_settings; 
};

struct ctrlproxy_config {
	char *config_dir;
	GList *networks;
	int separate_processes:1;
	char *replication;
	char *shared_path;
	char *linestack_backend;
};

/* config.c */
G_MODULE_EXPORT struct network_config *network_config_init(struct ctrlproxy_config *cfg);
G_MODULE_EXPORT void save_configuration(struct ctrlproxy_config *cfg, const char *name);
G_MODULE_EXPORT struct ctrlproxy_config *load_configuration(const char *name);
G_MODULE_EXPORT void free_config(struct ctrlproxy_config *);

#endif /* __CTRLPROXY_SETTINGS_H__ */
