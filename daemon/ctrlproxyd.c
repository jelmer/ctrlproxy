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

int current_log_level = 0;

#define DEFAULT_CONFIG_FILE "/etc/ctrlproxyd.conf"

int main(int argc, char **argv)
{
	GOptionContext *pc;
	const char *config_file = DEFAULT_CONFIG_FILE;
	GOptionEntry options[] = {
		{"config-file", 'c', 0, G_OPTION_ARG_STRING, &config_file, "Communicate with client to NETWORK via stdio", "NETWORK" },
		{"debug-level", 'd', 'd', G_OPTION_ARG_INT, &current_log_level, ("Debug level [0-5]"), "LEVEL" },
		{"version", 'v', 0, G_OPTION_ARG_NONE, &version, ("Show version information")},
		{ NULL }
	};
	GError *error;

	signal(SIGINT, signal_quit);
	signal(SIGTERM, signal_quit);
#ifdef SIGPIPE
	signal(SIGPIPE, SIG_IGN);
#endif
#ifdef SIGHUP
	signal(SIGHUP, signal_hup);
#endif
#ifdef SIGSEGV
	signal(SIGSEGV, signal_crash);
#endif
#ifdef SIGUSR1
	signal(SIGUSR1, signal_save);
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

	/* FIXME: Read configuration file */

	if (isdaemon) {
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
			log_global(LOG_ERROR, "Unable to daemonize\n");
			g_free(config_dir);
			return -1;
		}
		isdaemon = 1;
#else
		log_global(LOG_ERROR, "Daemon mode not compiled in");
		g_free(config_dir);
		return -1;
#endif
	} 

	write_pidfile(my_global);

	/* FIXME: Add listener */

	g_main_loop_run(main_loop);

	return 0;
}


