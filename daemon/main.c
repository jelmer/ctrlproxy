/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2006 Jelmer Vernooij <jelmer@nl.linux.org>

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
#include <glib/gstdio.h>

static int log_level = 0;

#define DEFAULT_CONFIG_FILE "/etc/ctrlproxyd.conf"
#define PIDFILE "/var/run/ctrlproxyd.pid"

struct ctrlproxyd_config {
	const char *port;
	const char *address;
};

struct ctrlproxyd_config *read_config_file(const char *name)
{
	struct ctrlproxyd_config *config;
	GError *error = NULL;
	GKeyFile *kf = g_key_file_new();

	if (!g_key_file_load_from_file(kf, name, G_KEY_FILE_NONE, &error)) {
		fprintf(stderr, "Unable to load '%s': %s\n", name, error->message);
		g_key_file_free(kf);
		return NULL;
	}

	config = g_new0(struct ctrlproxyd_config, 1);
	config->port = g_key_file_get_string(kf, "settings", "port", NULL);
	config->address = g_key_file_get_string(kf, "settings", "address", NULL);

	g_key_file_free(kf);

	return config;
}

void signal_crash(int sig)
{
	/* FIXME */
	exit(1);
}

void signal_quit(int sig)
{
	exit(0); /* FIXME */
}

int main(int argc, char **argv)
{
	struct ctrlproxyd_config *config;
	GOptionContext *pc;
	const char *config_file = DEFAULT_CONFIG_FILE;
	gboolean version = FALSE;
	gboolean inetd = FALSE;
	pid_t pid;
	GMainLoop *main_loop;
	GOptionEntry options[] = {
		{"config-file", 'c', 0, G_OPTION_ARG_STRING, &config_file, "Configuration file", "CONFIGFILE"},
		{"debug-level", 'd', 'd', G_OPTION_ARG_INT, &log_level, ("Debug level [0-5]"), "LEVEL" },
		{"inetd", 'I', 0, G_OPTION_ARG_NONE, &inetd, ("Run in inetd mode")},
		{"version", 'v', 0, G_OPTION_ARG_NONE, &version, ("Show version information")},
		{ NULL }
	};
	GError *error = NULL;

	signal(SIGINT, signal_quit);
	signal(SIGTERM, signal_quit);
#ifdef SIGPIPE
	signal(SIGPIPE, SIG_IGN);
#endif
#ifdef SIGHUP
	signal(SIGHUP, SIG_IGN);
#endif
#ifdef SIGSEGV
	signal(SIGSEGV, signal_crash);
#endif

	main_loop = g_main_loop_new(NULL, FALSE);

	pc = g_option_context_new("");
	g_option_context_add_main_entries(pc, options, NULL);

	if (!g_option_context_parse(pc, &argc, &argv, &error)) {
		fprintf(stderr, "%s\n", error->message);
		return 1;
	}

	if (version) {
		printf("ctrlproxy %s\n", VERSION);
		printf("(c) 2002-2008 Jelmer Vernooij et al. <jelmer@nl.linux.org>\n");
		return 0;
	}

	config = read_config_file(config_file);
	if (config == NULL) {
		return 1;
	}

	pid = read_pidfile(PIDFILE);
	if (pid != -1) {
		fprintf(stderr, "ctrlproxyd already running at pid %d!\n", pid);
		return 1;
	}

#ifdef HAVE_DAEMON 
#ifdef SIGTTOU
	signal(SIGTTOU, SIG_IGN);
#endif

#ifdef SIGTTIN
	signal(SIGTTIN, SIG_IGN);
#endif

#ifdef SIGTSTP
	signal(SIGTSTP, SIG_IGN);
#endif
	if (daemon(1, 0) < 0) {
		fprintf(stderr, "Unable to daemonize\n");
		return -1;
	}
#else
	fprintf(stderr, "Daemon mode not compiled in\n");
	return -1;
#endif

	if (inetd) {
		/* FIXME */
	} else { 
		write_pidfile(PIDFILE);

		/* FIXME: Add listener */
	}

	g_main_loop_run(main_loop);

	return 0;
}


