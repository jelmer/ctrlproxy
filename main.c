/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2003 Jelmer Vernooij <jelmer@nl.linux.org>

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

#define add_log_domain(domain) g_log_set_handler (domain, G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION, log_handler, NULL);

/* globals */
GMainLoop *main_loop;
FILE *debugfd = NULL;
FILE *f_logfile = NULL;
extern char my_hostname[];

void signal_crash(int sig) 
{
#ifdef HAVE_BACKTRACE_SYMBOLS
	void *backtrace_stack[BACKTRACE_STACK_SIZE];
	size_t backtrace_size;
	char **backtrace_strings;
#endif
	g_critical(("Received SIGSEGV!"));

#ifdef HAVE_BACKTRACE_SYMBOLS
	/* get the backtrace (stack frames) */
	backtrace_size = backtrace(backtrace_stack,BACKTRACE_STACK_SIZE);
	backtrace_strings = backtrace_symbols(backtrace_stack, backtrace_size);

	g_critical (("BACKTRACE: %d stack frames:"), backtrace_size);
	
	if (backtrace_strings) {
		int i;

		for (i = 0; i < backtrace_size; i++)
			g_critical(" #%u %s", i, backtrace_strings[i]);

		g_free(backtrace_strings);
	}

#endif
	g_error(("Ctrlproxy core has segfaulted, exiting..."));
	abort();
}

void log_handler(const gchar *log_domain, GLogLevelFlags flags, const gchar *message, gpointer user_data) {
	if(!f_logfile)return;
	fprintf(f_logfile, "%s\n", message);
	fflush(f_logfile);
}

static void clean_exit()
{
	extern gint ping_loop_id;
	GList *gl;

	kill_pending_clients();

	while((gl = get_network_list())) {
		struct network *n = (struct network *)gl->data;
		close_network(n);
	}

	if(debugfd)fclose(debugfd);
	fini_config();
	fini_plugins();

	g_source_remove(ping_loop_id);
	g_main_loop_quit(main_loop);

	g_main_loop_unref(main_loop);
}

void signal_quit(int sig)
{
	static int state = 0;
	g_message(("Received signal %d, quitting..."), sig);
	if(state == 1) { 
		signal(SIGINT, SIG_IGN); 
		exit(0);
	}

	state = 1;

	g_message(("Closing connections..."));

	exit(0);
}

void signal_save(int sig)
{
	g_message("Received USR1 signal, saving configuration...");
	save_configuration(NULL);
}

int main(int argc, const char *argv[])
{
	int isdaemon = 0;
	int seperate_processes = 0;
	char *logfile = NULL, *rcfile = NULL;
	char *configuration_file;
	const char *inetd_client = NULL;
#ifdef HAVE_POPT_H
	int c;
	poptContext pc;
	struct poptOption options[] = {
		POPT_AUTOHELP
		{"inetd-client", 'i', POPT_ARG_STRING, &inetd_client, 0, "Communicate with client to NETWORK via stdin/stdout", "NETWORK" },
		{"debug", 'd', POPT_ARG_STRING, NULL, 'd', ("Write irc traffic to specified file"), "FILE" },
		{"daemon", 'D', POPT_ARG_NONE, &isdaemon, 0, ("Run in the background (as a daemon)")},
		{"log", 'l', POPT_ARG_STRING, &logfile, 0, ("Log messages to specified file"), ("FILE")},
		{"rc-file", 'r', POPT_ARG_STRING, &rcfile, 0, ("Use configuration file from specified location"), ("FILE")},
		{"seperate-processes", 's', POPT_ARG_NONE, &seperate_processes, 0, ("Use one process per network") },
		{"version", 'v', POPT_ARG_NONE, NULL, 'v', ("Show version information")},
		POPT_TABLEEND
	};
#endif

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

#ifdef HAVE_POPT_H
	pc = poptGetContext(argv[0], argc, argv, options, 0);

	while((c = poptGetNextOpt(pc)) >= 0) {
		switch(c) {
		case 'd': 
			{
				const char *fname = poptGetOptArg(pc);
				if(!strcmp(fname, "-")) { debugfd = stdout; break; }
				debugfd = fopen(fname, "w+"); 
			}
			break;
		case 'v':
			printf("ctrlproxy %s\n", PACKAGE_VERSION);
			printf(("(c) 2002-2004 Jelmer Vernooij et al. <jelmer@nl.linux.org>\n"));
			return 0;
		}
	}
#endif

	if(gethostname(my_hostname, MAXHOSTNAMELEN) != 0) {
		g_error(("Can't figure out hostname of local host!"));
		return 1;
	}

	if(logfile) {
		f_logfile = fopen(logfile, "a+");
		if(!f_logfile) g_warning(("Can't open logfile %s!"), logfile);
	}

	add_log_domain("GLib");
	add_log_domain("ctrlproxy");

	g_message("Logfile opened");

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
#ifdef SIGHUP
		signal(SIGHUP, SIG_IGN);
#endif
		daemon(1, 0);
		isdaemon = 1;
#else
		g_warning("Daemon mode not compiled in");
#endif
	} else 
		if(!f_logfile)f_logfile = stdout;

	init_config();

	if(rcfile) {
		configuration_file = g_strdup(rcfile);
		load_configuration(configuration_file);
	} else { 
		const char *homedir = g_get_home_dir();
#ifdef _WIN32
		configuration_file = g_strdup_printf("%s/_ctrlproxyrc", homedir);
#else
		configuration_file = g_strdup_printf("%s/.ctrlproxyrc", homedir);
#endif
		/* Copy configuration file from default location if none existed yet */
		if(g_file_test(configuration_file, G_FILE_TEST_EXISTS)) {
			load_configuration(configuration_file);
		} else {
			load_configuration(SHAREDIR"/ctrlproxyrc.default");
		}

		g_free(configuration_file);
	}

#ifdef HAVE_POPT_H
	poptFreeContext(pc);
#endif

	atexit(clean_exit);

	if (inetd_client) {
		GIOChannel *io = g_io_channel_unix_new(0);
		struct network *n = find_network(inetd_client);

		if (!n) {
			fprintf(stderr, "Unable to find network named '%s'\n", inetd_client);
		} else {
			/* Find clients network by name */
			new_client(n, io);
		}
	}

	g_main_loop_run(main_loop);

	return 0;
}
