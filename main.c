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

#include "internals.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#define BACKTRACE_STACK_SIZE 64

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif

#define add_log_domain(domain) g_log_set_handler (domain, G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION, log_handler, NULL);

/* globals */
GMainLoop *main_loop;
char my_hostname[MAXHOSTNAMELEN+2];
char *configuration_file;
FILE *debugfd = NULL;
FILE *f_logfile = NULL;

const char *get_modules_path() { return MODULESDIR; }
const char *get_shared_path() { return SHAREDIR"/ctrlproxy"; }
const char *get_my_hostname() { return my_hostname; }
const char *ctrlproxy_version() { return PACKAGE_VERSION; }

void signal_crash(int sig) 
{
#ifdef HAVE_BACKTRACE_SYMBOLS
	void *backtrace_stack[BACKTRACE_STACK_SIZE];
	size_t backtrace_size;
	char **backtrace_strings;
#endif
	g_critical(_("Received SIGSEGV!"));

#ifdef HAVE_BACKTRACE_SYMBOLS
	/* get the backtrace (stack frames) */
	backtrace_size = backtrace(backtrace_stack,BACKTRACE_STACK_SIZE);
	backtrace_strings = backtrace_symbols(backtrace_stack, backtrace_size);

	g_critical (_("BACKTRACE: %d stack frames:"), backtrace_size);
	
	if (backtrace_strings) {
		int i;

		for (i = 0; i < backtrace_size; i++)
			g_critical(" #%u %s", i, backtrace_strings[i]);

		g_free(backtrace_strings);
	}

#endif
	
	if(peek_plugin()) {
		if(!unload_plugin(peek_plugin())) {
			g_error(_("Can't unload crashing plugin, exiting..."));
			abort();
		} else {
			g_warning(_("Plugin '%s' unloaded, because it crashed..."), peek_plugin()->name);
			pop_plugin();
		}
	} else {
		g_error(_("Ctrlproxy core has segfaulted, exiting..."));
		abort();
	}
}

void log_handler(const gchar *log_domain, GLogLevelFlags flags, const gchar *message, gpointer user_data) {
	if(!f_logfile)return;
	fprintf(f_logfile, "%s\n", message);
	fflush(f_logfile);
}

void clean_exit()
{
	GList *gl = get_network_list();
	while(gl) {
		struct network *n = (struct network *)gl->data;
		gl = gl->next;
		if(n) close_network(n);
	}
	if(debugfd)fclose(debugfd);
	g_main_loop_quit(main_loop);
}

void signal_quit(int sig)
{
	static int state = 0;
	g_message(_("Received signal %d, quitting..."), sig);
	if(state == 1) { 
		signal(SIGINT, SIG_IGN); 
		exit(0);
	}

	state = 1;

	g_message(_("Closing connections..."));
	g_main_loop_quit(main_loop);
}

void signal_save(int sig)
{
	save_configuration();
}

int main(int argc, const char *argv[])
{
	int isdaemon = 0;
	int seperate_processes = 0;
	char *logfile = NULL, *rcfile = NULL;
#ifdef HAVE_POPT_H
	int c;
	poptContext pc;
	struct poptOption options[] = {
		POPT_AUTOHELP
		{"debug", 'd', POPT_ARG_STRING, NULL, 'd', _("Write irc traffic to specified file"), "FILE" },
		{"daemon", 'D', POPT_ARG_NONE, &isdaemon, 0, _("Run in the background (as a daemon)")},
		{"log", 'l', POPT_ARG_STRING, &logfile, 0, _("Log messages to specified file"), _("FILE")},
		{"rc-file", 'r', POPT_ARG_STRING, &rcfile, 0, _("Use configuration file from specified location"), _("FILE")},
		{"seperate-processes", 's', POPT_ARG_NONE, &seperate_processes, 0, _("Use one process per network") },
		{"version", 'v', POPT_ARG_NONE, NULL, 'v', _("Show version information")},
		POPT_TABLEEND
	};
#endif

	bindtextdomain(PACKAGE_NAME, LOCALEDIR);
	textdomain(PACKAGE_NAME);

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
			printf(_("(c) 2002-2004 Jelmer Vernooij et al. <jelmer@nl.linux.org>\n"));
			return 0;
		}
	}
#endif

	if(gethostname(my_hostname, MAXHOSTNAMELEN) != 0) {
		g_error(_("Can't figure out hostname of local host!"));
		return 1;
	}

	if(logfile) {
		f_logfile = fopen(logfile, "a+");
		if(!f_logfile) g_warning(_("Can't open logfile %s!"), logfile);
	}

	add_log_domain("GLib");
	add_log_domain("ctrlproxy");
	add_filter_class("", -1);
	add_filter_class("client", 100);
	add_filter_class("replicate", 50);
	add_filter_class("log", 50);

	g_message(_("Logfile opened"));

#ifdef daemon
	if(isdaemon) {

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
	} else 
#endif
		if(!f_logfile)f_logfile = stdout;

	if(rcfile) {
		configuration_file = g_strdup(rcfile);
		readConfig(configuration_file);
	} else { 
		const char *homedir = g_get_home_dir();
#ifdef _WIN32
		asprintf(&configuration_file, "%s/_ctrlproxyrc", homedir);
#else
		asprintf(&configuration_file, "%s/.ctrlproxyrc", homedir);
#endif
		/* Copy configuration file from default location if none existed yet */
		if(g_file_test(configuration_file, G_FILE_TEST_EXISTS)) {
			readConfig(configuration_file);
		} else {
			readConfig(SHAREDIR"/ctrlproxyrc.default");
		}
	}

	if(!init_plugins() || !init_networks()) return -1;
	initialized_hook_execute();
	g_main_loop_run(main_loop);
	clean_exit();

	return 0;
}


