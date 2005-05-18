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
	if (!wait_response(fd, "001")) {
		fprintf(stderr, "No 001 sent after login\n");
		return -1;
	}
	
	if (!wait_response(fd, "002")) {
		fprintf(stderr, "No 002 sent after login\n");
		return -1;
	}
	
	if (!wait_response(fd, "003")) {
		fprintf(stderr, "No 003 sent after login\n");
		return -1;
	}
	
	if (!wait_response(fd, "004")) {
		fprintf(stderr, "No 004 sent after login\n");
		return -1;
	}
	g_io_channel_unref(fd);
	return 0;
}

static int test_registerfirst(void)
{
	GIOChannel *fd = new_conn();
	irc_send_args(fd, "PRIVMSG", "a", "a", NULL);
	if (!wait_response(fd, "451")) 
		return -1;
	g_io_channel_unref(fd);
	return 0;
}



static int test_user_needmoreparams(void)
{
	GIOChannel *fd = new_conn();
	irc_send_args(fd, "USER", "a", "a", NULL);
	if (!wait_response(fd, "461")) 
		return -1;
	g_io_channel_unref(fd);
	return 0;
}

static int test_userhost_needmoreparams(void)
{
	GIOChannel *fd = new_conn_loggedin();
	if (!fd) return -1;
	irc_send_args(fd, "USERHOST", NULL);
	if (!wait_response(fd, "461")) 
		return -1;
	g_io_channel_unref(fd);
	return 0;
}

static int test_motd(void)
{
	GIOChannel *fd = new_conn();
	const char *resp[] = { "375", "422", NULL };
	irc_send_args(fd, "USER", "a", "a", "a", "a", NULL);
	irc_send_args(fd, "NICK", "bla", NULL);
	if (!wait_responses(fd, resp)) 
		return -1;
	g_io_channel_unref(fd);
	return 0;
}

static int test_userhost(void)
{
	GIOChannel *fd = new_conn_loggedin();
	if (!fd) return -1;
	irc_send_args(fd, "USERHOST", "bla", NULL);
	if (!wait_response(fd, "302")) {
			g_io_channel_unref(fd);
			return -1;
	}
	g_io_channel_unref(fd);
	return 0;
}

static int test_who_simple(void)
{
	GIOChannel *fd = new_conn_loggedin();
	if (!fd) return -1;
	irc_send_args(fd, "WHO", "bla", NULL);
	if (!wait_response(fd, "315")) {
			g_io_channel_unref(fd);
			return -1;
	}
	g_io_channel_unref(fd);
	return 0;
}



static int test_selfmessage(void)
{
	GIOChannel *fd = new_conn_loggedin();
	irc_send_args(fd, "PRIVMSG", "bla", "MyMessage", NULL);
	do { 
		struct line *l = wait_response(fd, "PRIVMSG");
		
		if (!l) {
			g_io_channel_unref(fd);
			return -1;
		}
		
		if (!g_strcasecmp(l->args[1], "bla") && 
			!strcmp(l->args[2], "MyMessage")) {
			g_io_channel_unref(fd);
			return 0;
		}
		
	} while(1);

	return -1;
}

void simple_init(void)
{
	register_test("CONNECT", test_connect);
	register_test("LOGIN", test_login);
	register_test("MOTD", test_motd);
	register_test("ERR-REGISTERFIRST", test_registerfirst);
	register_test("USERHOST", test_userhost);
	register_test("SELFMESSAGE", test_selfmessage);
	register_test("USER-NEEDMOREPARAMS", test_user_needmoreparams);
	register_test("USERHOST-NEEDMOREPARAMS", test_userhost_needmoreparams);
	register_test("WHO-SIMPLE", test_who_simple);
}
