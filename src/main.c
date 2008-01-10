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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "internals.h"
#include <glib/gstdio.h>

#define BACKTRACE_STACK_SIZE 64

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <netdb.h>

#include "help.h"

/* globals */
static GMainLoop *main_loop;
extern char my_hostname[];
extern struct global *my_global;
extern help_t *help; 

static void signal_hup(int sig)
{
	char *logfile;
	logfile = g_build_filename(my_global->config->config_dir, "log", NULL);
	init_log(logfile);
}

static void signal_crash(int sig) 
{
#ifdef HAVE_BACKTRACE_SYMBOLS
	void *backtrace_stack[BACKTRACE_STACK_SIZE];
	size_t backtrace_size;
	char **backtrace_strings;
#endif
	log_global(LOG_ERROR, "Received SIGSEGV!");

#ifdef HAVE_BACKTRACE_SYMBOLS
	/* get the backtrace (stack frames) */
	backtrace_size = backtrace(backtrace_stack,BACKTRACE_STACK_SIZE);
	backtrace_strings = backtrace_symbols(backtrace_stack, backtrace_size);

	log_global(LOG_ERROR, "BACKTRACE: %d stack frames:", backtrace_size);
	
	if (backtrace_strings) {
		int i;

		for (i = 0; i < backtrace_size; i++)
			log_global(LOG_ERROR, " #%u %s", i, backtrace_strings[i]);

		g_free(backtrace_strings);
	}

#endif
	log_global(LOG_ERROR, "Please send a bug report to jelmer@vernstok.nl.");
	log_global(LOG_ERROR, "A gdb backtrace is appreciated if you can reproduce this bug.");
	abort();
}

static void clean_exit()
{
	char *path;
	kill_pending_clients("Server exiting");

	g_main_loop_quit(main_loop);

	path = my_global->config->config_dir;

	if (!g_file_test(path, G_FILE_TEST_IS_DIR)) {
		if (g_mkdir(path, 0700) != 0) {
			log_global(LOG_ERROR, "Can't create config directory '%s': %s", path, strerror(errno));
			return;
		}
	}

	config_save_notify(my_global, path);
	if (my_global->config->autosave)
		save_configuration(my_global->config, path);
	nickserv_save(my_global, path);
	stop_unix_socket(my_global);
	stop_admin_socket(my_global);
	fini_listeners(my_global);
	free_global(my_global);

	g_main_loop_unref(main_loop);
}

static void signal_quit(int sig)
{
	static int state = 0;
	log_global(LOG_WARNING, "Received signal %d, quitting...", sig);
	if (state == 1) { 
		signal(SIGINT, SIG_IGN); 
		exit(0);
	}

	state = 1;

	exit(0);
}

static void signal_save(int sig)
{
	log_global(LOG_INFO, "Received USR1 signal, saving configuration...");
	
	if (!g_file_test(my_global->config->config_dir, G_FILE_TEST_IS_DIR)) {
		if (g_mkdir(my_global->config->config_dir, 0700) != 0) {
			log_global(LOG_ERROR, "Can't create config directory '%s': %s", my_global->config->config_dir, strerror(errno));
			return;
		}
	}

	config_save_notify(my_global, my_global->config->config_dir);
	global_update_config(my_global);
	save_configuration(my_global->config, my_global->config->config_dir);
	nickserv_save(my_global, my_global->config->config_dir);
}

static pid_t read_pidfile(struct global *global)
{
	char *path = g_build_filename(global->config->config_dir, "pid", NULL);
	char *contents;
	pid_t pid;
	GError *error = NULL;
	if (!g_file_get_contents(path, &contents, NULL, &error)) 
		return -1;
	g_free(path);
	pid = atol(contents);
	/* FIXME: Check if pid is still running */
	g_free(contents);
	return pid;
}

static gboolean write_pidfile(struct global *global)
{
	char *path = g_build_filename(global->config->config_dir, "pid", NULL);
	GError *error = NULL;
	char contents[100];
	snprintf(contents, 100, "%u", getpid());
	if (!g_file_set_contents(path, contents, -1, &error)) {
		log_global(LOG_ERROR, "Unable to write pid file `%s'", path);
		return FALSE;
	}
	g_free(path);
	return TRUE;
}

int main(int argc, char **argv)
{
	int isdaemon = 0;
	char *logfile = NULL;
	extern enum log_level current_log_level;
	extern gboolean no_log_timestamp;
	char *config_dir = NULL;
	char *tmp;
	gboolean init = FALSE;
	const char *inetd_client = NULL;
	gboolean version = FALSE;
	GOptionContext *pc;
	GOptionEntry options[] = {
		{"inetd-client", 'i', 0, G_OPTION_ARG_STRING, &inetd_client, "Communicate with client to NETWORK via stdio", "NETWORK" },
		{"debug-level", 'd', 'd', G_OPTION_ARG_INT, &current_log_level, ("Debug level [0-5]"), "LEVEL" },
		{"no-timestamp", 'n', 0, G_OPTION_ARG_NONE, &no_log_timestamp, "No timestamps in logs" },
		{"daemon", 'D', 0, G_OPTION_ARG_NONE, &isdaemon, ("Run in the background (as a daemon)")},
		{"init", 0, 0, G_OPTION_ARG_NONE, &init, "Create configuration" },
		{"log", 'l', 0, G_OPTION_ARG_STRING, &logfile, ("Log messages to specified file"), ("FILE")},
		{"config-dir", 'c', 0, G_OPTION_ARG_STRING, &config_dir, ("Override configuration directory"), ("DIR")},
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
		printf("(c) 2002-2007 Jelmer Vernooij et al. <jelmer@nl.linux.org>\n");
		return 0;
	}

	if (config_dir == NULL) 
		config_dir = g_build_filename(g_get_home_dir(), ".ctrlproxy", NULL);

	if (isdaemon && !logfile) {
		logfile = g_build_filename(config_dir, "log", NULL);
	}

	if (init) {
		if (!create_configuration(config_dir)) {
			g_free(config_dir);
			return 1;
		}
		printf("Configuration created in %s. \n", config_dir);
		g_free(config_dir);
		return 0;
	}

	init_log(logfile);

	log_global(LOG_INFO, "CtrlProxy %s starting", VERSION);

	if (gethostname(my_hostname, NI_MAXHOST) != 0) {
		log_global(LOG_WARNING, "Can't figure out hostname of local host!");
		g_free(config_dir);
		return 1;
	}

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

	init_admin();
	init_nickserv();
	init_replication();
	help = help_load_file(HELPFILE);

	/* Determine correct modules directory */
	init_plugins(getenv("CTRLPROXY_MODULESDIR")?getenv("CTRLPROXY_MODULESDIR"):MODULESDIR);

	tmp = g_build_filename(config_dir, "config", NULL);

	if (!g_file_test(tmp, G_FILE_TEST_EXISTS)) {
		char *rcfile;

		rcfile = g_build_filename(g_get_home_dir(), ".ctrlproxyrc", NULL);
		
		if (g_file_test(rcfile, G_FILE_TEST_EXISTS)) {
			log_global(LOG_INFO, "Pre-3.0 style .ctrlproxyrc found");
			log_global(LOG_INFO, "Run ctrlproxy-upgrade to update configuration");
			g_free(config_dir);
			g_free(rcfile);
			return 1;
		} else {
			log_global(LOG_INFO, "No configuration found. Maybe you would like to create one by running with --init ?");
			g_free(config_dir);
			g_free(rcfile);
			return 1;
		}

		g_free(rcfile);
	} else {
		my_global = load_global(config_dir);	
	}
	g_free(config_dir);
	g_free(tmp);

	if (my_global == NULL) {
		log_global(LOG_ERROR, "Unable to load configuration, exiting...");
		return 1;
	}

	write_pidfile(my_global);

	start_unix_socket(my_global);
	start_admin_socket(my_global);
	autoconnect_networks(my_global->networks);
	if (!init_listeners(my_global)) {
		log_global(LOG_ERROR, "Failed to start one or more listeners, exiting...");
		return 1;
	}
	if (my_global->config->auto_away.enabled)
		auto_away_add(my_global, &my_global->config->auto_away);
	g_option_context_free(pc);

	atexit(clean_exit);

	if (inetd_client) {
		GIOChannel *io = g_io_channel_unix_new(0);
		struct irc_network *n = find_network(my_global->networks, inetd_client);

		if (!n) {
			fprintf(stderr, "Unable to find network named '%s'\n", inetd_client);
			return 1;
		} else {
			/* Find clients network by name */
			struct client *client = client_init(n, io, "Standard I/O");
			client->exit_on_close = TRUE;
		}
	}

	g_main_loop_run(main_loop);


	return 0;
}
