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

#include "ircdtorture.h"
#include <stdio.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#define RANDOMDEVICE "/dev/urandom"
#define REPEAT_COUNT 20

static int test_random_data(void)
{
	GIOChannel *g = new_conn_loggedin("bla");
	int fd1;
	int fd2 = open(RANDOMDEVICE, O_RDONLY);
	off_t off = 0;

	if (!g) 
		return -1;

	fd1 = g_io_channel_unix_get_fd(g);

	if (fd2 == -1) {
		fprintf(stderr, "Can't open %s: %s\n", RANDOMDEVICE, strerror(errno));
		return -1;
	}

	if (sendfile(fd1, fd2, &off, REPEAT_COUNT * 0x1000) == -1)
		return -1;
	
	close(fd2);

	g_io_channel_close(g);
	return 1;
}

static int test_random_msg(void)
{
	GIOChannel *g = new_conn_loggedin("bla");
	const char *cmds[] = { "PRIVMSG", "NICK", NULL };
	int i;

	for (i = 0; cmds[i]; i++) {
		irc_send_args(g, cmds[i], NULL);
		irc_send_args(g, cmds[i], "bla", NULL);
		irc_send_args(g, cmds[i], "bla", "bla", NULL);
		irc_send_args(g, cmds[i], "bla", "bla", "bla", NULL);
		irc_send_args(g, cmds[i], "bla", "bla", "bla", "bla", NULL);
		irc_send_args(g, cmds[i], "bla", "bla", "bla", "bla", "bla", NULL);
	}

	return 0;
}

void ircdtorture_init(void)
{
	register_test("RANDOM-DATA", test_random_data);
	register_test("RANDOM-MSG", test_random_msg);
}
