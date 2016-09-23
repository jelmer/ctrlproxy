/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2006 Jelmer Vernooĳ <jelmer@jelmer.uk>

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

#ifndef HAVE_DAEMON_USER_H
#define HAVE_DAEMON_USER_H

struct ctrlproxyd_config;

struct daemon_user {
	 char *configdir;
	 char *pidpath;
	 char *socketpath;
	 char *username;
	 gint last_status;
	 gint child_watch;
	 uid_t uid; /* -1 if not a system user */
	 GPid pid;
	 struct irc_listener *listener;
};

gboolean daemon_user_exists(struct daemon_user *user);
gboolean daemon_user_running(struct daemon_user *user);
void daemon_user_free(struct daemon_user *user);
struct daemon_user *get_daemon_user(struct ctrlproxyd_config *config, const char *username);
gboolean daemon_user_start(struct daemon_user *user, const char *ctrlproxy_path, struct irc_listener *l);
void foreach_daemon_user(struct ctrlproxyd_config *config, struct irc_listener *listener,
						 void (*fn) (struct daemon_user *user, const char *ctrlproxy_path, struct irc_listener *l));
struct daemon_user *domain_user_init(struct ctrlproxyd_config *config, const char *username);

#endif
