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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "internals.h"
#include "daemon/user.h"
#include <pwd.h>
#include "daemon/daemon.h"

gboolean daemon_user_exists(struct daemon_user *user)
{
	return g_file_test(user->configdir, G_FILE_TEST_IS_DIR);
}

gboolean daemon_user_running(struct daemon_user *user)
{
	user->pid = read_pidfile(user->pidpath);
	
	return (user->pid != -1);
}

void daemon_user_free(struct daemon_user *user)
{
	if (user == NULL)
		return;
	g_free(user->pidpath);
	g_free(user->socketpath);
	g_free(user->configdir);
	g_free(user->username);
	g_free(user);
}

struct daemon_user *get_daemon_user(struct ctrlproxyd_config *config, const char *username)
{
	struct daemon_user *user = g_new0(struct daemon_user, 1);
	struct passwd *pwd;

	user->username = g_strdup(username);
	user->child_watch = -1;

	if (config->configdir != NULL) {
		user->configdir = g_build_filename(config->configdir, username, NULL);
		user->uid = -1;
	} else {
		pwd = getpwnam(username);

		if (pwd == NULL) {
			g_free(user->username);
			g_free(user);
			return NULL;
		}

		user->configdir = g_build_filename(pwd->pw_dir, ".ctrlproxy", NULL);
		user->uid = pwd->pw_uid;
	}

	user->socketpath = g_build_filename(user->configdir, "socket", NULL);
	user->pidpath = g_build_filename(user->configdir, "pid", NULL);
	return user;
}

struct spawn_data {
	struct daemon_user *user;
	struct irc_listener *listener;
};

static void user_setup(gpointer user_data)
{
	struct spawn_data *data = user_data;
	if (setuid(data->user->uid) < 0) {
		listener_log(LOG_WARNING, data->listener, "Unable to change effective user id to %s (%d): %s",
					 data->user->username, data->user->uid,
					 strerror(errno));
		exit(1);
	}	
}

static void daemon_user_exits(GPid pid, gint status, gpointer data)
{
	struct daemon_user *user = (struct daemon_user *)data;

	listener_log((status == 0)?LOG_INFO:LOG_WARNING, user->listener,
					 "ctrlproxy instance for '%s' exited with status %d",
					 user->configdir, status);

	user->last_status = status;

	g_spawn_close_pid(pid);
	user->pid = -1;
	user->child_watch = -1;
}


gboolean daemon_user_start(struct daemon_user *user, const char *ctrlproxy_path, struct irc_listener *l)
{
	GError *error = NULL;
	struct spawn_data spawn_data;
	char **command;
	int child_stdin, child_stdout, child_stderr;
	spawn_data.listener = l;
	spawn_data.user = user;

	user->listener = l;

	command = g_new0(char *, 6);
	command[0] = g_strdup(ctrlproxy_path);
	command[1] = g_strdup("--config-dir");
	command[2] = g_strdup(user->configdir);
	if (user->uid == (uid_t)-1) {
		command[3] = g_strdup("--restricted");
		command[4] = NULL;
	} else {
		command[3] = NULL;
	}

	if (!g_spawn_async_with_pipes(NULL, command, NULL, G_SPAWN_SEARCH_PATH|G_SPAWN_DO_NOT_REAP_CHILD, user_setup, &spawn_data,
				  &user->pid, &child_stdin, &child_stdout, &child_stderr, &error)) {
		listener_log(LOG_WARNING, l, "Unable to start ctrlproxy for %s (%s): %s", user->username,
					 user->configdir, error->message);
		g_error_free(error);
		return FALSE;
	}
	g_strfreev(command);

	listener_log(LOG_INFO, l, "Launched new ctrlproxy instance for %s at %s",
				 user->username, user->configdir);
	user->child_watch = g_child_watch_add(user->pid, daemon_user_exits, user);

	/* FIXME: What if the process hasn't started up completely while we try to
	 * connect to the socket? */

	return TRUE;
}

void foreach_daemon_user(struct ctrlproxyd_config *config, struct irc_listener *listener,
						 void (*fn) (struct daemon_user *user, const char *ctrlproxy_path, struct irc_listener *l))
{
	struct daemon_user *user;

	if (config->configdir == NULL) {
		struct passwd *pwent;
		setpwent();
		while ((pwent = getpwent()) != NULL) {
			user = get_daemon_user(config, pwent->pw_name);
			fn(user, config->ctrlproxy_path, listener);
		}
		endpwent();
	} else {
		const char *name;
		GDir *dir = g_dir_open(config->configdir, 0, NULL);
		if (dir == NULL)
			return;
		while ((name = g_dir_read_name(dir)) != NULL) {
			user = get_daemon_user(config, name);
			fn(user, config->ctrlproxy_path, listener);
		}
		g_dir_close(dir);
	}
}

struct daemon_user *domain_user_init(struct ctrlproxyd_config *config, const char *username)
{
	/* FIXME: */
	return NULL;
}

