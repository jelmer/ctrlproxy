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

#define _GNU_SOURCE
#include "ircdtorture.h"

#ifdef HAVE_POPT_H
#include <popt.h>
#endif

#include <glib.h>
#include "../ctrlproxy.h"

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

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
	GIOStatus status;
	struct line *l = NULL;

	/* FIXME: Timeout */
	do { 
		int i;
		status = irc_recv_line(ch, &error, &l);

		if (status == G_IO_STATUS_NORMAL) {
			for (i = 0; cmd[i]; i++) {
				if (!strcmp(l->args[0], cmd[i])) 
					return l;
			}
		}
	} while(status != G_IO_STATUS_ERROR);
	
	return l;
}

GList *tests = NULL;

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

	fcntl(sock[0], F_SETFL, O_NONBLOCK);

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
static const char **args = NULL;
static const char *port = "6667";

GIOChannel *new_conn(void)
{
	pid_t child_pid;
	int std_fd;
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

GIOChannel *new_conn_loggedin(void)
{
	GIOChannel *fd = new_conn();
	
	irc_send_args(fd, "USER", "a", "a", "a", "a", NULL);
	irc_send_args(fd, "NICK", "bla", NULL);
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

void simple_init(void);

int main(int argc, const char *argv[])
{
	GList *gl;

#ifdef HAVE_POPT_H
	int c;
	poptContext pc;
	struct poptOption options[] = {
		POPT_AUTOHELP
		{"ip", 'I', POPT_ARG_STRING, &ip, 't', "Connect to specified host rather then using stdout/stdin", "HOST"},
		{"tcp-port", 'p', POPT_ARG_STRING, &port, 'p', "Connect to specified TCP port (implies -I)", "PORT" },
		{"version", 'v', POPT_ARG_NONE, NULL, 'v', "Show version information"},
		POPT_TABLEEND
	};
#endif

#ifdef HAVE_POPT_H
	pc = poptGetContext(argv[0], argc, argv, options, 0);

	poptSetOtherOptionHelp(pc, "[ircdtorture options] -- /path/to/ircd [arguments...]");

	while((c = poptGetNextOpt(pc)) >= 0) {
		switch(c) {
		case 'v':
			printf("ircdtorture %s\n", PACKAGE_VERSION);
			printf(("(c) 2005 Jelmer Vernooij et al. <jelmer@nl.linux.org>\n"));
			return 0;
		}
	}
#endif
	
	if (!poptPeekArg(pc)) {
		poptPrintUsage(pc, stderr, 0);
		return -1;
	}

	args = poptGetArgs(pc);

	simple_init();

	for (gl = tests; gl; gl = gl->next) {
		if (run_test(gl->data)) 
			return -1;
	}

	return 0;
}
