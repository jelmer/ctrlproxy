#include "internals.h"

char my_hostname[MAXHOSTNAMELEN+2];

void signal_quit(int sig)
{
	close_all();
	sleep(5);
	exit(0);
}

int main(int argc, const char *argv[])
{
	struct server *s;
	char *t;
	char *nick, *port, *host, *fullname, *pass, *mods;
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
		POPT_TABLEEND
	};

	signal(SIGINT, signal_quit);
	signal(SIGTERM, signal_quit);

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
	
	t = first_section();

	while(t) {
		nick = get_conf(t, "nick");
		if(!nick)nick = getenv("USER");
		port = get_conf(t, "port");
		if(!port)port = "6667";
		host = get_conf(t, "host");
		if(!host) {
			fprintf(stderr, "No host specified for %s !\n", t);
			return 1;
		}
		fullname = get_conf(t, "fullname");
		pass = get_conf(t, "password");
		
		s = connect_to_server(host, atoi(port), nick, pass);
		s->abbrev = t;
		autojoin = get_conf(t, "autojoin");
		if(autojoin)server_send(s, NULL, "JOIN", autojoin, NULL);
		
		if(!s)return 1;

		mods = get_conf(t, "modules");
		t = next_section(t);
		if(!mods)continue;
		
		while((tmp = strchr(mods, ' ')) || (tmp = strchr(mods, '\0'))) {
			if(*tmp == '\0')final = 1;
			*tmp = '\0';
			
			load_module(s, mods);

			if(final)break;
			mods = tmp+1;
		}
	}
	
	if(daemon) {
		if(fork() != 0)exit(0);
	}
	
	while(1) {
		loop_all();
	}
}
