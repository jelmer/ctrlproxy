/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2006 Jelmer Vernooij <jelmer@nl.linux.org>

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

	path = _global->config->config_dir;
	config_save_notify(_global, path);
	if (_global->config->autosave)
		save_configuration(_global->config, path);
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
	config_save_notify(_global, _global->config->config_dir);
	save_configuration(_global->config, _global->config->config_dir);
}

struct global *new_global(const char *config_dir)
{
	struct global *global = g_new0(struct global, 1);

	global->config = load_configuration(config_dir);

	if (!global->config) {
		g_free(global);
		return NULL;
	}

	config_load_notify(global);
	
	return global;
}

void free_global(struct global *global)
{
	free_linestack_context(global->linestack); global->linestack = NULL;
	fini_networks(global);
}

static GList *load_notifies = NULL, *save_notifies = NULL;

void register_load_config_notify(config_load_notify_fn fn)
{
	load_notifies = g_list_append(load_notifies, fn);
}

void register_save_config_notify(config_save_notify_fn fn)
{
	save_notifies = g_list_append(save_notifies, fn);
}

void config_load_notify(struct global *global)
{
	GList *gl;
	for (gl = load_notifies; gl; gl = gl->next) {
		config_load_notify_fn fn = gl->data;

		fn(global);
	}
}

void config_save_notify(struct global *global, const char *dest)
{
	GList *gl;
	for (gl = save_notifies; gl; gl = gl->next) {
		config_save_notify_fn fn = gl->data;

		fn(global, dest);
	}
}

int main(int argc, char **argv)
{
	int isdaemon = 0;
	char *logfile = NULL;
	extern enum log_level current_log_level;
	extern gboolean no_log_timestamp;
	const char *config_dir = NULL;
	char *tmp;
	const char *inetd_client = NULL;
	gboolean version = FALSE;
	GOptionContext *pc;
	GOptionEntry options[] = {
		{"inetd-client", 'i', 0, G_OPTION_ARG_STRING, &inetd_client, "Communicate with client to NETWORK via stdio", "NETWORK" },
		{"debug-level", 'd', 'd', G_OPTION_ARG_INT, &current_log_level, ("Debug level [0-5]"), "LEVEL" },
		{"no-timestamp", 'n', FALSE, G_OPTION_ARG_NONE, &no_log_timestamp, "No timestamps in logs" },
		{"daemon", 'D', 0, G_OPTION_ARG_NONE, &isdaemon, ("Run in the background (as a daemon)")},
		{"log", 'l', 0, G_OPTION_ARG_STRING, &logfile, ("Log messages to specified file"), ("FILE")},
		{"config-dir", 'c', 0, G_OPTION_ARG_STRING, &config_dir, ("Override configuration directory"), ("DIR")},
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
		printf("(c) 2002-2006 Jelmer Vernooij et al. <jelmer@nl.linux.org>\n");
		return 0;
	}

	if (config_dir == NULL) 
		config_dir = g_build_filename(g_get_home_dir(), ".ctrlproxy", NULL);

	if(isdaemon && !logfile) {
		logfile = g_build_filename(config_dir, "log", NULL);
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

	init_plugins(getenv("MODULESDIR")?getenv("MODULESDIR"):MODULESDIR);

	tmp = g_build_filename(config_dir, "config", NULL);

	if (!g_file_test(tmp, G_FILE_TEST_EXISTS)) {
		char *rcfile = g_build_filename(g_get_home_dir(), ".ctrlproxyrc", NULL);
		
		if(g_file_test(rcfile, G_FILE_TEST_EXISTS)) {
			log_global(NULL, LOG_INFO, "Pre-3.0 style .ctrlproxyrc found, starting upgrade");
			/* FIXME: Upgrade script */
		} else {
			log_global(NULL, LOG_INFO, "No configuration found, installing one");
			setup_configdir(config_dir);
		}

		g_free(rcfile);
	}
	g_free(tmp);

	_global = new_global(config_dir);	
	
	if (_global == NULL) {
		log_global(NULL, LOG_ERROR, "Unable to load configuration, exiting...");
		return 1;
	}

	load_networks(_global, _global->config);

	/* Determine correct modules directory */
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
