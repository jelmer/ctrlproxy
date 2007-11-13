/*
    ircdtorture: an IRC RFC compliancy tester
	(c) 2006 Jelmer Vernooij <jelmer@nl.linux.org>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <check.h>
#include "ctrlproxy.h"
#include "torture.h"

START_TEST(test_create_no_network)
	GIOChannel *ch = g_io_channel_unix_new(0);
	client_init(NULL, ch, "desc");
END_TEST

START_TEST(test_create_introduction)
	GIOChannel *ch1, *ch2;
	g_io_channel_pair(&ch1, &ch2);
	client_init(NULL, ch1, "desc");
END_TEST

START_TEST(test_network_first)
	GIOChannel *ch1, *ch2;
	struct client *c;
	char *raw;
	g_io_channel_pair(&ch1, &ch2);
	c = client_init(NULL, ch1, "desc");
	g_io_channel_unref(ch1);
	fail_unless(g_io_channel_write_chars(ch2, "NICK bla\r\n"
				"USER a a a a\r\n", -1, NULL, NULL) == G_IO_STATUS_NORMAL);
	fail_unless(g_io_channel_flush(ch2, NULL) == G_IO_STATUS_NORMAL);
	g_main_iteration(FALSE);
	g_io_channel_read_to_end(ch2, &raw, NULL, NULL);
	fail_unless(!strcmp(raw, "ERROR :Please select a network first, or specify one in your configuration file\r\n"), "Got: %s", raw);
END_TEST


START_TEST(test_login)
	GIOChannel *ch1, *ch2;
	struct client *c;
	struct global *g = TORTURE_GLOBAL;
	struct network n = { 
		.info.name = "test",
		.global = g,
	};
	g_io_channel_pair(&ch1, &ch2);
	c = client_init(&n, ch1, "desc");
	g_io_channel_unref(ch1);
	fail_unless(g_io_channel_write_chars(ch2, "NICK bla\r\n"
				"USER a a a a\r\n", -1, NULL, NULL) == G_IO_STATUS_NORMAL);
	fail_unless(g_io_channel_flush(ch2, NULL) == G_IO_STATUS_NORMAL);
	g_main_iteration(FALSE);
	fail_if(c->state->me.nick == NULL);
	fail_unless(!strcmp(c->state->me.nick, "bla"));
END_TEST

START_TEST(test_read_nonutf8)
	GIOChannel *ch1, *ch2;
	struct client *c;
	struct global *g = TORTURE_GLOBAL;
	struct network n = { 
		.info.name = "test",
		.global = g,
	};
	g_io_channel_pair(&ch1, &ch2);
	c = client_init(&n, ch1, "desc");
	g_io_channel_unref(ch1);
	fail_unless(g_io_channel_write_chars(ch2, "NICK bla\r\n"
				"USER a a a a\r\n"
				"PRIVMSG foo :èeeé\\r\n", -1, NULL, NULL) == G_IO_STATUS_NORMAL);
	fail_unless(g_io_channel_flush(ch2, NULL) == G_IO_STATUS_NORMAL);
	g_main_iteration(FALSE);
	fail_if(c->state->me.nick == NULL);
	fail_unless(!strcmp(c->state->me.nick, "bla"));
	client_send_args(c, "PRIVMSG", "foo", "\x024:öéé", NULL);
END_TEST



START_TEST(test_disconnect)
	GIOChannel *ch1, *ch2;
	struct client *client;
	char *raw;
	GError *error = NULL;
	gsize length;
	g_io_channel_pair(&ch1, &ch2);
	client = client_init(NULL, ch1, "desc");
	g_io_channel_unref(ch1);
	disconnect_client(client, "Because");
	g_io_channel_read_to_end(ch2, &raw, &length, &error);
	fail_unless(!strcmp(raw, "ERROR :Because\r\n"));
END_TEST

START_TEST(test_login_nonetwork)
	GIOChannel *ch1, *ch2;
	struct client *c;
	g_io_channel_pair(&ch1, &ch2);
	c = client_init(NULL, ch1, "desc");
	g_io_channel_unref(ch1);
	fail_unless(g_io_channel_write_chars(ch2, "NICK bla\r\n"
				"USER a a a a\r\n", -1, NULL, NULL) == G_IO_STATUS_NORMAL);
	fail_unless(g_io_channel_flush(ch2, NULL) == G_IO_STATUS_NORMAL);
	g_main_iteration(FALSE);
	fail_if(c->state->me.nick == NULL);
END_TEST



Suite *client_suite()
{
	Suite *s = suite_create("client");
	TCase *tc_core = tcase_create("core");
	suite_add_tcase(s, tc_core);
	tcase_add_test(tc_core, test_create_no_network);
	tcase_add_test(tc_core, test_create_introduction);
	tcase_add_test(tc_core, test_disconnect);
	tcase_add_test(tc_core, test_login);
	tcase_add_test(tc_core, test_login_nonetwork);
	tcase_add_test(tc_core, test_network_first);
	tcase_add_test(tc_core, test_read_nonutf8);
	return s;
}
