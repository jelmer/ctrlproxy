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
#include <unistd.h>

static int test_connect(void)
{
	GIOChannel *fd = new_conn();
	g_io_channel_unref(fd);
	if (fd >= 0) return 0; else return -1;
}

static int test_login(void)
{
	GIOChannel *fd = new_conn();
	irc_send_args(fd, "USER", "a", "a", "a", "a", NULL);
	irc_send_args(fd, "NICK", "bla", NULL);
	g_io_channel_unref(fd);
	return 0;
}

void simple_init(void)
{
	register_test("IRC-CONNECT", test_connect);
	register_test("IRC-LOGIN", test_login);
}
