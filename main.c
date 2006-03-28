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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "internals.h"

#define BACKTRACE_STACK_SIZE 64

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <netdb.h>

/* globals */
static GMainLoop *main_loop;
extern char my_hostname[];

struct global *_global;

static void signal_crash(int sig) 
{
#ifdef HAVE_BACKTRACE_SYMBOLS
	void *backtrace_stack[BACKTRACE_STACK_SIZE];
	size_t backtrace_size;
	char **backtrace_strings;
#endif
	log_global(NULL, LOG_ERROR, "Received SIGSEGV!");

#ifdef HAVE_BACKTRACE_SYMBOLS
	/* get the backtrace (stack frames) */
	backtrace_size = backtrace(backtrace_stack,BACKTRACE_STACK_SIZE);
	backtrace_strings = backtrace_symbols(backtrace_stack, backtrace_size);

	log_global(NULL, LOG_ERROR, "BACKTRACE: %d stack frames:", backtrace_size);
	
	if (backtrace_strings) {
		int i;

		for (i = 0; i < backtrace_size; i++)
			log_global(NULL, LOG_ERROR, " #%u %s", i, backtrace_strings[i]);

		g_free(backtrace_strings);
	}

#endif
	log_global(NULL, LOG_ERROR, "Please send a bug report to jelmer@vernstok.nl.");
	log_global(NULL, LOG_ERROR, "A gdb backtrace is appreciated if you can reproduce this bug.");
	log_global(NULL, LOG_ERROR, "Ctrlproxy core has segfaulted, exiting...");
	abort();
}

static void clean_exit()
{
	char *path;
	kill_pending_clients("Server exiting");

	fini_networks(_global);

	g_main_loop_quit(main_loop);

	path = g_build_filename(_global->config->config_dir, "autosave", NULL);
	save_configuration(_global->config, path);
	g_free(path);
	free_config(_global->config);

	free_global(_global);

	g_main_loop_unref(main_loop);
	fini_log();
}

static void signal_quit(int sig)
{
	static int state = 0;
	log_global(NULL, LOG_WARNING, "Received signal %d, quitting...", sig);
	if(state == 1) { 
		signal(SIGINT, SIG_IGN); 
		exit(0);
	}

	state = 1;

	exit(0);
}

static void signal_save(int sig)
{
	log_global(NULL, LOG_INFO, "Received USR1 signal, saving configuration...");
	save_configuration(_global->config, _global->last_config_file);
}

struct global *new_global()
{
	struct global *global = g_new0(struct global, 1);

	return global;
}

void free_global(struct global *global)
{
	free_linestack_context(global->linestack); global->linestack = NULL;
	fini_networks(global);
}

static GList *notifies = NULL;

void register_config_notify(config_notify_fn fn)
{
	notifies = g_list_append(notifies, fn);
}

void config_notify(struct global *global)
{
	GList *gl;
	for (gl = notifies; gl; gl = gl->next) {
		config_notify_fn fn = gl->data;

		fn(global);
	}
}

int main(int argc, char **argv)
{
	int isdaemon = 0;
	char *logfile = NULL, *rcfile = NULL;
	char *configuration_file;
	extern enum log_level current_log_level;
	extern gboolean no_log_timestamp;
	char *config_dir;
	const char *inetd_client = NULL;
	gboolean version = FALSE;
	GOptionContext *pc;
	GOptionEntry options[] = {
		{"inetd-client", 'i', 0, G_OPTION_ARG_STRING, &inetd_client, "Communicate with client to NETWORK via stdio", "NETWORK" },
		{"debug-level", 'd', 'd', G_OPTION_ARG_INT, &current_log_level, ("Debug level [0-5]"), "LEVEL" },
		{"no-timestamp", 'n', FALSE, G_OPTION_ARG_NONE, &no_log_timestamp, "No timestamps in logs" },
		{"daemon", 'D', 0, G_OPTION_ARG_NONE, &isdaemon, ("Run in the background (as a daemon)")},
		{"log", 'l', 0, G_OPTION_ARG_STRING, &logfile, ("Log messages to specified file"), ("FILE")},
		{"rc-file", 'r', 0, G_OPTION_ARG_STRING, &rcfile, ("Use configuration file from specified location"), ("FILE")},
		{"version", 'v', 'v', G_OPTION_ARG_NONE, &version, ("Show version information")},
		{ NULL }
	};

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
#ifdef SIGUSR1
	signal(SIGUSR1, signal_save);
#endif

	main_loop = g_main_loop_new(NULL, FALSE);

	pc = g_option_context_new("");
	g_option_context_add_main_entries(pc, options, NULL);

	if(!g_option_context_parse(pc, &argc, &argv, NULL))
		return 1;

	if (version) {
		printf("ctrlproxy %s\n", VERSION);
		printf("(c) 2002-2005 Jelmer Vernooij et al. <jelmer@nl.linux.org>\n");
		return 0;
	}

	if(isdaemon && !logfile) {
		logfile = g_build_filename(_global->config->config_dir, "log", NULL);
	}

	init_log(logfile);

	log_global(NULL, LOG_INFO, "CtrlProxy %s starting", VERSION);

	if(gethostname(my_hostname, MAXHOSTNAMELEN) != 0) {
		log_global(NULL, LOG_WARNING, "Can't figure out hostname of local host!");
		return 1;
	}

	if(isdaemon) {

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
		daemon(1, 0);
		isdaemon = 1;
#else
		log_global(NULL, LOG_ERROR, "Daemon mode not compiled in");
		return -1;
#endif
	} 

	_global = new_global();	

	config_dir = g_build_filename(g_get_home_dir(), ".ctrlproxy", NULL);
	if(mkdir(config_dir, 0700) != 0 && errno != EEXIST) {
		log_global(NULL, LOG_ERROR, "Unable to open configuration directory '%s'\n", config_dir);
		g_free(config_dir);
		config_dir = NULL;
	}

	if(rcfile) {
		configuration_file = g_strdup(rcfile);
		_global->config = load_configuration(configuration_file, config_dir);
		_global->last_config_file = configuration_file;
	} else { 
#ifdef _WIN32
		configuration_file = g_build_filename(g_get_home_dir(), "_ctrlproxyrc", NULL);
#else
		configuration_file = g_build_filename(g_get_home_dir(), ".ctrlproxyrc", NULL);
#endif
		/* Copy configuration file from default location if none existed yet */
		if(g_file_test(configuration_file, G_FILE_TEST_EXISTS)) {
			_global->config = load_configuration(configuration_file, config_dir);
			_global->last_config_file = configuration_file;
		} else {
			g_free(configuration_file);
			configuration_file = g_build_filename(SHAREDIR, "ctrlproxyrc.default", NULL);
			_global->config = load_configuration(configuration_file, config_dir);
			_global->last_config_file = configuration_file;
		}
	}


	load_networks(_global, _global->config);
	config_notify(_global);

	/* Determine correct modules directory */

	init_plugins(getenv("MODULESDIR")?getenv("MODULESDIR"):MODULESDIR);
	_global->linestack = new_linestack(_global->config);
	autoconnect_networks(_global);

	g_option_context_free(pc);

	atexit(clean_exit);

	if (inetd_client) {
		GIOChannel *io = g_io_channel_unix_new(0);
		struct network *n = find_network(_global, inetd_client);

		if (!n) {
			fprintf(stderr, "Unable to find network named '%s'\n", inetd_client);
		} else {
			/* Find clients network by name */
			struct client *client = client_init(n, io, "Standard I/O");
			client->exit_on_close = TRUE;
		}
	}

	g_main_loop_run(main_loop);

	return 0;
}
