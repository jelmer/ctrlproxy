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

#include <unistd.h>

#define BACKTRACE_STACK_SIZE 64

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif

#define add_log_domain(domain) g_log_set_handler (domain, G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION, log_handler, NULL);

/* globals */
GMainLoop *main_loop;
char my_hostname[MAXHOSTNAMELEN+2];
xmlNodePtr xmlNode_networks = NULL, xmlNode_plugins = NULL;
xmlDocPtr configuration;
char *configuration_file;
FILE *debugfd = NULL;
FILE *f_logfile = NULL;
struct plugin *current_plugin = NULL;

GList *plugins = NULL;

void signal_crash(int sig) 
{
#ifdef HAVE_BACKTRACE_SYMBOLS
	void *backtrace_stack[BACKTRACE_STACK_SIZE];
	size_t backtrace_size;
	char **backtrace_strings;
#endif
	g_critical("Received SIGSEGV!");

#ifdef HAVE_BACKTRACE_SYMBOLS
	/* get the backtrace (stack frames) */
	backtrace_size = backtrace(backtrace_stack,BACKTRACE_STACK_SIZE);
	backtrace_strings = backtrace_symbols(backtrace_stack, backtrace_size);

	g_critical ("BACKTRACE: %d stack frames:", backtrace_size);
	
	if (backtrace_strings) {
		int i;

		for (i = 0; i < backtrace_size; i++)
			g_critical(" #%u %s", i, backtrace_strings[i]);

		free(backtrace_strings);
	}

#endif
	
	if(current_plugin) {
		if(!unload_plugin(current_plugin)) {
			g_error("Can't unload crashing plugin, exiting...");
			abort();
		} else {
			g_warning("Plugin '%s' unloaded, because it crashed...", current_plugin->name);
		}
	} else {
		g_error("Ctrlproxy core has segfaulted, exiting...");
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
	GList *gl = networks;
	while(gl) {
		struct network *n = (struct network *)gl->data;
		gl = gl->next;
		if(n) close_network(n);
	}
	if(debugfd)fclose(debugfd);
}

void signal_quit(int sig)
{
	static int state = 0;
	g_log(G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE, "Received signal %d, quitting...", sig);
	if(state == 1) { 
		signal(SIGINT, SIG_IGN); 
		exit(0);
	}

	state = 1;

	g_log(G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE, "Closing connections...");
	g_main_loop_quit(main_loop);
}

gboolean unload_plugin(struct plugin *p)
{
	plugin_fini_function f;

	/* Run exit function if present */
	if(g_module_symbol(p->module, "fini_plugin", (gpointer)&f)) {
		if(!f(p)) {
			g_warning("Unable to unload plugin '%s': still in use?", p->name);
			return FALSE;
		}
	} else {
		g_warning("No symbol 'fini_plugin' in plugin '%s'. Module does not support unloading, so no unload will be attempted", p->name);
		return FALSE;
	}

	g_module_close(p->module);

	/* Remove autoload from this plugins' element */
	xmlSetProp(p->xmlConf, "autoload", "0");
	return TRUE;
}

gboolean plugin_loaded(char *name)
{
	GList *gl = plugins;
	while(gl) {
		struct plugin *p = (struct plugin *)gl->data;
		if(!strcmp(p->name, name)) return TRUE;
		gl = gl->next;
	}
	return FALSE;
}

gboolean load_plugin(xmlNodePtr cur)
{
	GModule *m;
	char *mod_name;
	struct plugin *p;
	char *modulesdir;
	gchar *path_name;
	plugin_init_function f = NULL;

	mod_name = xmlGetProp(cur, "file");
	if(!mod_name) {
		g_warning("No filename specified for plugin");
		return FALSE;
	}

	if(plugin_loaded(mod_name)) {
		g_warning("Plugin already loaded");
		return FALSE;
	}

	/* Determine correct modules directory */
	if(getenv("MODULESDIR"))modulesdir = getenv("MODULESDIR");
	else modulesdir = MODULESDIR;

	if(mod_name[0] == '/')path_name = g_strdup(mod_name);
	else path_name = g_module_build_path(modulesdir, mod_name);


	m = g_module_open(path_name, 0);
	if(!m) {
		g_warning("Unable to open module %s(%s), ignoring", path_name, g_module_error());
		xmlFree(mod_name);
		g_free(path_name);
		return FALSE;
	} else {
		if(!g_module_symbol(m, "init_plugin", (gpointer)&f)) {
			g_warning("Can't find symbol 'init_plugin' in module %s", path_name);
			g_free(path_name);
			return FALSE;
		}
	}

	g_free(path_name);

	p = malloc(sizeof(struct plugin));
	p->name = strdup(mod_name);
	p->module = m;
	p->xmlConf = cur;

	if(!f(p)) {
		g_warning("Running initialization function for plugin '%s' failed.", mod_name);
		free(p->name);
		free(p);
		return FALSE;
	}

	plugins = g_list_append(plugins, p);
	
	xmlSetProp(cur, "autoload", "1");
	xmlFree(mod_name);
	return TRUE;
}

void save_configuration()
{
	xmlSaveFile(configuration_file, configuration);
}

void signal_save(int sig)
{
	save_configuration();
}

void readConfig(char *file) {
    xmlNodePtr cur;

	configuration = xmlParseFile(file);
	if(!configuration) {
		g_error("Can't open configuration file '%s'", file);
		exit(1);
	}

	cur = xmlDocGetRootElement(configuration);
	g_assert(cur);

	g_assert(!strcmp(cur->name, "ctrlproxy"));

	cur = cur->xmlChildrenNode;
	while(cur) {
		if(xmlIsBlankNode(cur) || !strcmp(cur->name, "comment")) {
			cur = cur->next;
			continue;
		}
		
		if(!strcmp(cur->name, "plugins")) {
			xmlNode_plugins = cur;
		} else if(!strcmp(cur->name, "networks")) {
			xmlNode_networks = cur;
		} else g_assert(0);
		
		cur = cur->next;
	}
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
		{"debug", 'd', POPT_ARG_STRING, NULL, 'd', "Write irc traffic to specified file", "FILE" },
		{"daemon", 'D', POPT_ARG_NONE, &isdaemon, 0, "Run in the background (as a daemon)"},
		{"log", 'l', POPT_ARG_STRING, &logfile, 0, "Log messages to specified file", "FILE"},
		{"rc-file", 'r', POPT_ARG_STRING, &rcfile, 0, "Use configuration file from specified location", "FILE"},
		{"seperate-processes", 's', POPT_ARG_NONE, &seperate_processes, 0, "Use one process per network" },
		{"version", 'v', POPT_ARG_NONE, NULL, 'v', "Show version information"},
		POPT_TABLEEND
	};
#endif
    xmlNodePtr cur;

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
			printf("(c) 2002-2003 Jelmer Vernooij et al. <jelmer@nl.linux.org>\n");
			return 0;
		}
	}
#endif

	if(gethostname(my_hostname, MAXHOSTNAMELEN) != 0) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, "Can't figure out hostname of local host!");
		return 1;
	}

	if(logfile) {
		f_logfile = fopen(logfile, "a+");
		if(!f_logfile) g_warning("Can't open logfile %s!", logfile);
	}

	add_log_domain("GLib");
	add_log_domain("ctrlproxy");
	add_filter_class(NULL, -1);
	add_filter_class("client", 100);

	g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "Logfile opened");

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
		signal(SIGHUP, SIG_IGN);
		daemon(1, 0);
		isdaemon = 1;
	} else if(!f_logfile)f_logfile = stdout;

	if(rcfile) configuration_file = strdup(rcfile);
	else { 
		const char *homedir = g_get_home_dir();
#ifdef _WIN32
		asprintf(&configuration_file, "%s/_ctrlproxyrc", homedir);
#else
		asprintf(&configuration_file, "%s/.ctrlproxyrc", homedir);
#endif
	}

	readConfig(configuration_file);

	if(!g_module_supported()) {
		g_warning("DSO's not supported on this platform. Not loading any modules");
	} else if(!xmlNode_plugins) {
		g_warning("No modules set to be loaded");	
	}else {
		cur = xmlNode_plugins->xmlChildrenNode;
		while(cur) {
			char *enabled;

			if(xmlIsBlankNode(cur) || !strcmp(cur->name, "comment")){ cur = cur->next; continue; }

			g_assert(!strcmp(cur->name, "plugin"));

			enabled = xmlGetProp(cur, "autoload");
			if((!enabled || atoi(enabled) == 1) && !load_plugin(cur)) {
				g_error("Can't load plugin %s, aborting...", xmlGetProp(cur, "file"));
				abort();
			}

			xmlFree(enabled);

			cur = cur->next;
		}
	}

	if(xmlNode_networks) {
		g_error("No networks listed");
		return 1;
	}

	cur = xmlNode_networks->xmlChildrenNode;
	while(cur) {
		char *autoconnect;
		if(xmlIsBlankNode(cur) || !strcmp(cur->name, "comment")){ cur = cur->next; continue; }
		g_assert(!strcmp(cur->name, "network"));

		autoconnect = xmlGetProp(cur, "autoconnect");
		if(autoconnect && !strcmp(autoconnect, "1")) {
			if(seperate_processes) { 
				if(fork() == 0) {  
					connect_network(cur); 
					break; 
				}
			} else {
				connect_network(cur);
			}
		}
		xmlFree(autoconnect);

		cur = cur->next;
	}


	g_timeout_add(1000 * 300, ping_loop, NULL);
	initialized_hook_execute();
	if(networks) g_main_loop_run(main_loop);
	clean_exit();

	return 0;
}


