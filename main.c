/* 
	ctrlproxy: A modular IRC proxy
	(c) 2002 Jelmer Vernooij <jelmer@nl.linux.org>

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

char my_hostname[MAXHOSTNAMELEN+2];
char output_debug = 0;

void signal_crash(int sig) 
{
	/* Just loop until someone runs gdb on us */
	while(1)sleep(5);
}

void signal_quit(int sig)
{
	close_all();
	sleep(5);
	exit(0);
}

int main(int argc, const char *argv[])
{
	struct server *s;
	char **sections;
	int i;
	char *t;
	char *nick, *port, *host, *pass, *mods, *modsdup;
	char *tmp;
	char *autojoin;
	int final = 0;
	poptContext pc;
	char daemon = 0;
	char *rcfile_default, *rcfile;
	int c;
	struct poptOption options[] = {
		POPT_AUTOHELP
		{"rc-file", 'r', POPT_ARG_STRING, &rcfile, 0, "Use alternative ctrlproxyrc file", "RCFILE"},
		{"daemon", 'D', POPT_ARG_VAL, &daemon, 1, "Run in the background (as a daemon)"},
		{"version", 'v', POPT_ARG_NONE, NULL, 'v', "Show version information"},
		{"debug", 'd', POPT_ARG_VAL, &output_debug, 1, "Output debug information"},
		POPT_TABLEEND
	};

	signal(SIGINT, signal_quit);
	signal(SIGTERM, signal_quit);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGSEGV, signal_crash);

	asprintf(&rcfile_default, "%s/.ctrlproxyrc", getenv("HOME")?getenv("HOME"):"");
	rcfile = rcfile_default;

	pc = poptGetContext(argv[0], argc, argv, options, 0);

	while((c = poptGetNextOpt(pc)) >= 0) {
		switch(c) {
		case 'v':
			printf("ctrlproxy %s\n", CTRLPROXY_VERSION);
			printf("(c) 2002 Jelmer Vernooij <jelmer@nl.linux.org>\n");
			exit(1);
			break;
		}
	}

	if(gethostname(my_hostname, MAXHOSTNAMELEN) != 0) {
		fprintf(stderr, "Can't figure out hostname of local host!\n");
		return 1;
	}

	if(load_conf_file(rcfile) == -1)
		return 0;
	
	sections = enum_sections();

	for(i = 0; sections[i]; i++) {
		t = sections[i];
		nick = get_conf(t, "nick");
		if(!nick)nick = getenv("USER");
		port = get_conf(t, "port");
		if(!port)port = "6667";
		host = get_conf(t, "host");
		if(!host) {
			fprintf(stderr, "No host specified for %s !\n", t);
			return 1;
		}
		pass = get_conf(t, "password");
		
		s = connect_to_server(host, atoi(port), nick, pass);
		if(!s)return 1;

		s->abbrev = t;

		/* Do autojoin stuff */
		autojoin = get_conf(t, "autojoin");
		if(autojoin)server_send(s, NULL, "JOIN", autojoin, NULL);

		/* Load modules */
		mods = get_conf(t, "modules");
		if(!mods)continue;
		modsdup = strdup(mods);
		
		final = 0;
		mods = modsdup;
		while((tmp = strchr(mods, ' ')) || (tmp = strchr(mods, '\0'))) {
			if(*tmp == '\0')final = 1;
			*tmp = '\0';
			
			load_module(s, mods);

			if(final)break;
			mods = tmp+1;
		}
		free(modsdup);
	}
	
	if(daemon) {
		int fd;
#ifdef SIGTTOU
		signal(SIGTTOU, SIG_IGN);
#endif

#ifdef SIGTTIN
		signal(SIGTTIN, SIG_IGN);
#endif

#ifdef SIGTSTP
		signal(SIGTSTP, SIG_IGN);
#endif

		if(fork() != 0)exit(0);

		setsid();
		signal(SIGHUP, SIG_IGN);
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);

	}

	while(1) {
		loop_all();
	}
}
