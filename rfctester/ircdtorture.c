/*
    ircdtorture: an IRC RFC compliancy tester
    (c) 2005 Jelmer Vernooij <jelmer@nl.linux.org>

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

#include "ircdtorture.h"

#include <glib.h>
#include <gmodule.h>
#include "../ctrlproxy.h"

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netdb.h>
#include <signal.h>

#define DEFAULT_TIMEOUT 3000

static gboolean dump = FALSE;
static gboolean debug = FALSE;

struct torture_test {
	const char *name;
	int (*test) (void);
};

struct line *wait_response(GIOChannel *ch, const char *cmd)
{
	const char *cmds[] = { cmd, NULL };
	return wait_responses(ch, cmds);
}

struct line *wait_responses(GIOChannel *ch, const char *cmd[])
{
	GError *error = NULL;
	GIOStatus status = G_IO_STATUS_AGAIN;
	struct line *l = NULL;

	/* FIXME: Timeout */
	do { 
		int i, ret;
		struct pollfd pl;

		pl.fd = g_io_channel_unix_get_fd(ch);
		pl.events = POLLIN | POLLERR | POLLHUP;

		ret = poll(&pl, 1, DEFAULT_TIMEOUT);

		if (ret == -1) {
			fprintf(stderr, "poll failed: %s ", strerror(errno));
			return NULL;
		}

		if (ret == 0) {
			fprintf(stderr, "poll timed out ");
			return NULL;
		}

		if (pl.revents & POLLIN)  {
			while ((status = irc_recv_line(ch, &error, &l)) == G_IO_STATUS_NORMAL) {

				if (l->argc == 0)
					continue;
				if (debug) printf("%s ", l->args[0]);
				if (dump) printf("%s\n", irc_line_string(l));

				for (i = 0; cmd[i]; i++) {
					if (!strcmp(l->args[0], cmd[i])) 
						return l;
				}
			}
		} else if (pl.revents & POLLHUP) {
			fprintf(stderr, "remote hup ");
			return NULL;
		} else if (pl.revents & POLLERR) {
			fprintf(stderr, "remote err ");
			return NULL;
		}

	} while(status != G_IO_STATUS_ERROR && 
		status != G_IO_STATUS_EOF);
	
	return l;
}

static GList *tests = NULL;

void register_test(const char *name, int (*data) (void)) 
{
	struct torture_test *test = g_new0(struct torture_test, 1);
	test->name = g_strdup(name);
	test->test = data;
	tests = g_list_append(tests, test);
}

static pid_t piped_child(char *const command[], int *f_in)
{
	pid_t pid;
	int sock[2];

	if(socketpair(PF_UNIX, SOCK_STREAM, AF_LOCAL, sock) == -1) {
		perror( "socketpair:");
		return -1;
	}

	*f_in = sock[0];

	pid = fork();
	if (pid == -1) {
		perror("fork");
		return -1;
	}

	if (pid == 0) {
		close(0);
		close(1);
		close(2);
		close(sock[0]);

		dup2(sock[1], 0);
		dup2(sock[1], 1);
		execvp(command[0], command);
		fprintf(stderr, "Failed to exec %s : %s", command[0], strerror(errno));
		return -1;
	}

	close(sock[1]);

	return pid;
}

static const char *ip = NULL;
static char **args = NULL;
static const char *port = "6667";

GIOChannel *new_conn(void)
{
	pid_t child_pid;
	int std_fd = -1;
	int fd;

	/* Start the program in question */
	
	child_pid = piped_child(args, &std_fd);
	
	if (!ip) {
		fd = std_fd;
	} else {
		struct addrinfo hints;
		struct addrinfo *real;
		int error;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = PF_INET;
		hints.ai_socktype = SOCK_STREAM;

		error = getaddrinfo(ip, port, &hints, &real);
		if (error) {
			fprintf(stderr, "Error during getaddrinfo: %s", gai_strerror(errno));

			return NULL;
		} 
		
		fd = socket(real->ai_family, real->ai_socktype, real->ai_protocol);
		if (!fd) {
			perror("socket");
			return NULL;
		}

		if (connect(fd, real->ai_addr, real->ai_addrlen) == -1) {
			perror("connect");
			return NULL;
		}
	}

	return g_io_channel_unix_new(fd);
}

GIOChannel *new_conn_loggedin(const char *nick)
{
	GIOChannel *fd = new_conn();

	if (!fd) 
		return NULL;
	
	irc_send_args(fd, "USER", "a", "a", "a", "a", NULL);
	irc_send_args(fd, "NICK", nick, NULL);

	if (!wait_response(fd, "001")) 
		return NULL;

	return fd;
}

int run_test(struct torture_test *test)
{
	int ret;
	printf("Running %s... ", test->name);
	fflush(stdout);
	ret = test->test();
	if (ret) printf("failed!\n"); else printf("ok\n");
	return ret;
}

gboolean load_module(const char *name)
{
	GModule *m;
	void (*init_func) (void);

	m = g_module_open(name, G_MODULE_BIND_LAZY);

	if(!m) {
		fprintf(stderr, "Can't open module %s: %s\n", name, g_module_error());
		return FALSE;
	}

	if(!g_module_symbol(m, "ircdtorture_init", (gpointer)&init_func)) {
		fprintf(stderr, "Can't find symbol \"ircdtorture_init\" in %s: %s\n", name, g_module_error());
		return FALSE;
	}

	init_func();
	return TRUE;
}

int main(int argc, char **argv)
{
	GList *gl;
	int ret = 0, i;
	gboolean version = FALSE;

	char **modules = NULL;
	GOptionContext *pc;
	const GOptionEntry options[] = {
		{"ip", 'I', 't', G_OPTION_ARG_STRING, &ip, "Connect to specified host rather then using stdout/stdin", "HOST"},
		{"tcp-port", 'p', 'p', G_OPTION_ARG_STRING, &port, "Connect to specified TCP port (implies -I)", "PORT" },
		{"module", 'm', 'm', G_OPTION_ARG_STRING_ARRAY, &modules, "Test module to load", "MODULE.so" },
		{"version", 'v', 'v', G_OPTION_ARG_NONE, &version, "Show version information"},
		{"debug", 'd', TRUE, G_OPTION_ARG_NONE, &debug, "Turn on debugging"},
		{"dump", 'D', TRUE, G_OPTION_ARG_NONE, &dump, "Print incoming traffic to stdout"},
		{ NULL }
	};
	
	pc = g_option_context_new("[ircdtorture options] -- /path/to/ircd [arguments...]");
	g_option_context_add_main_entries(pc, options, NULL);

	if(!g_option_context_parse(pc, &argc, &argv, NULL))
		return 1;

	if (version) {
		printf("ircdtorture %s\n", PACKAGE_VERSION);
		printf("(c) 2005 Jelmer Vernooij et al. <jelmer@nl.linux.org>\n");
		return 0;
	}

	for (i = 0; modules && modules[i];i++) {
		load_module(modules[i]);
	}

	signal(SIGPIPE, SIG_IGN);
	
	if (!argv[1] && !ip) {
		printf("No argument specified\n");
		return -1;
	}

	args = argv;

	for (gl = tests; gl; gl = gl->next) {
		if (run_test(gl->data)) 
			ret = -1;
	}

	return ret;
}
