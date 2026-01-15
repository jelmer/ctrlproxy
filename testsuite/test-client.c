/*
    ircdtorture: an IRC RFC compliance tester
	(c) 2006 Jelmer Vernooĳ <jelmer@jelmer.uk>

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
#include <string.h>
#include <check.h>
#include "ctrlproxy.h"
#include "torture.h"
#include "internals.h"

START_TEST(test_create_no_network)
{
	GIOChannel *ch = g_io_channel_unix_new(0);
	client_init_iochannel(NULL, ch, "desc");
}
END_TEST

START_TEST(test_create_introduction)
{
	GIOChannel *ch1, *ch2;
	g_io_channel_pair(&ch1, &ch2);
	client_init_iochannel(NULL, ch1, "desc");
}
END_TEST

START_TEST(test_network_first)
{
	GIOChannel *ch1, *ch2;
	struct irc_client *c;
	char *raw;
	g_io_channel_pair(&ch1, &ch2);
	c = client_init_iochannel(NULL, ch1, "desc");
	fail_unless(!strcmp(c->description, "desc"));
	g_io_channel_unref(ch1);
	fail_unless(g_io_channel_write_chars(ch2, "NICK bla\r\n"
				"USER a a a a\r\n", -1, NULL, NULL) == G_IO_STATUS_NORMAL);
	fail_unless(g_io_channel_flush(ch2, NULL) == G_IO_STATUS_NORMAL);
	g_main_iteration(FALSE);
	g_main_iteration(FALSE);
	g_main_iteration(FALSE);
	g_io_channel_read_to_end(ch2, &raw, NULL, NULL);
	fail_unless(!strcmp(raw, "ERROR :Please select a network first, or specify one in your configuration file\r\n"), "Got: %s", raw);
}
END_TEST


START_TEST(test_login)
{
	GIOChannel *ch1, *ch2;
	struct irc_client *c;
	struct global *g = TORTURE_GLOBAL;
	struct irc_network_info ni = { };
	struct irc_network n = {
		.info = &ni,
		.name = "test",
		.global = g,
	};
	g_io_channel_pair(&ch1, &ch2);
	c = client_init_iochannel(&n, ch1, "desc");
	g_io_channel_unref(ch1);
	fail_unless(g_io_channel_write_chars(ch2, "NICK bla\r\n"
				"USER a a a a\r\n", -1, NULL, NULL) == G_IO_STATUS_NORMAL);
	fail_unless(g_io_channel_flush(ch2, NULL) == G_IO_STATUS_NORMAL);
	fail_unless(g_io_channel_flush(ch1, NULL) == G_IO_STATUS_NORMAL);
	g_main_iteration(FALSE);
	fail_if(c->state == NULL);
	fail_if(c->state->me.nick == NULL);
	fail_unless(!strcmp(c->state->me.nick, "bla"));
}
END_TEST

START_TEST(test_read_nonutf8)
{
	GIOChannel *ch1, *ch2;
	struct irc_client *c;
	struct global *g = TORTURE_GLOBAL;
	struct irc_network_info ni = { .name = "test" };
	struct irc_network n = {
		.info = &ni,
		.global = g,
	};
	g_io_channel_pair(&ch1, &ch2);
	c = client_init_iochannel(&n, ch1, "desc");
	g_io_channel_unref(ch1);
	fail_unless(g_io_channel_write_chars(ch2, "NICK bla\r\n"
				"USER a a a a\r\n"
				"PRIVMSG foo :èeeé\\r\n", -1, NULL, NULL) == G_IO_STATUS_NORMAL);
	fail_unless(g_io_channel_flush(ch2, NULL) == G_IO_STATUS_NORMAL);
	g_main_iteration(FALSE);
	fail_if(c->state->me.nick == NULL);
	fail_unless(!strcmp(c->state->me.nick, "bla"));
	client_send_args(c, "PRIVMSG", "foo", "\x024:öéé", NULL);
}
END_TEST



START_TEST(test_disconnect)
{
	GIOChannel *ch1, *ch2;
	struct irc_client *client;
	char *raw;
	GError *error = NULL;
	gsize length;
	g_io_channel_pair(&ch1, &ch2);
	client = client_init_iochannel(NULL, ch1, "desc");
	g_io_channel_unref(ch1);
	client_disconnect(client, "Because");
	g_main_iteration(FALSE);
	g_main_iteration(FALSE);
	g_io_channel_read_to_end(ch2, &raw, &length, &error);
	fail_unless(!strcmp(raw, "ERROR :Because\r\n"));
}
END_TEST

START_TEST(test_replace_hostmask)
{
	struct irc_line *nl, *l;
	struct network_nick old = {
		.nick = "foo",
		.hostname = "foohost",
		.username = "foouser",
		.hostmask = "foo!foouser@foohost"
	};
	struct network_nick new = {
		.nick = "foo",
		.hostname = "barhost",
		.username = "baruser",
		.hostmask = "foo!baruser@barhost"
	};
	l = irc_parse_line(":foo!foouser@foohost JOIN #bar");
	fail_unless(NULL == irc_line_replace_hostmask(l, NULL, &old, &old));
	nl = irc_line_replace_hostmask(l, NULL, &old, &new);
	fail_if(nl == NULL);
	fail_unless(!strcmp(irc_line_string(nl), ":foo!baruser@barhost JOIN #bar"));
	l = irc_parse_line(":bar!lala@host JOIN #bar");
	fail_unless(NULL == irc_line_replace_hostmask(l, NULL, &old, &new));
	l = irc_parse_line(":server 302 foo :unused=+unused@host foo=+foouser@foohost");
	nl = irc_line_replace_hostmask(l, NULL, &old, &new);
	fail_if(nl == NULL);
	fail_unless(!strcmp(irc_line_string(nl), ":server 302 foo :unused=+unused@host foo=+baruser@barhost"), "was %s", irc_line_string(nl));
	l = irc_parse_line(":server 311 foo foo foouser foohost :someinfo");
	nl = irc_line_replace_hostmask(l, NULL, &old, &new);
	fail_if(nl == NULL);
	fail_unless(!strcmp(irc_line_string(nl), ":server 311 foo foo baruser barhost :someinfo"), "was %s", irc_line_string(nl));
	l = irc_parse_line("NOTICE * boe");
	nl = irc_line_replace_hostmask(l, NULL, &old, &new);
	fail_unless(nl == NULL);
}
END_TEST

START_TEST(test_login_nonetwork)
{
	GIOChannel *ch1, *ch2;
	struct irc_client *c;
	g_io_channel_pair(&ch1, &ch2);
	c = client_init_iochannel(NULL, ch1, "desc");
	g_io_channel_unref(ch1);
	fail_unless(g_io_channel_write_chars(ch2, "NICK bla\r\n"
				"USER a a a a\r\n", -1, NULL, NULL) == G_IO_STATUS_NORMAL);
	fail_unless(g_io_channel_flush(ch2, NULL) == G_IO_STATUS_NORMAL);
	g_main_iteration(FALSE);
	fail_if(c->state->me.nick == NULL);
}
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
	tcase_add_test(tc_core, test_replace_hostmask);
	return s;
}
