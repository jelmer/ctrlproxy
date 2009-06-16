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

#include "internals.h"
#include <stdio.h>
#include <string.h>
#include <check.h>
#include "torture.h"

START_TEST(test_create)
	struct network_config nc = {
		.name = "test"
	};
	struct irc_network *n;
	n = load_network(NULL, &nc);
END_TEST

START_TEST(test_uncreate)
	struct network_config nc = {
		.name = "test"
	};
	struct irc_network *n;
	n = load_network(NULL, &nc);
	unload_network(n);
END_TEST

START_TEST(test_login)
	GIOChannel *ch1, *ch2;
	struct network_config nc = {
		.name = "test",
		.nick = g_strdup("foo"),
		.username = g_strdup("blah"),
		.fullname = g_strdup("bloeh "),
		.type = NETWORK_IOCHANNEL
	};
	struct irc_network *n;
	struct global *g = TORTURE_GLOBAL;
	char *raw;
	GError *error = NULL;
	n = load_network(g, &nc);
	fail_unless(n->connection.state == NETWORK_CONNECTION_STATE_NOT_CONNECTED);
	g_io_channel_pair(&ch1, &ch2);
	network_set_iochannel(n, ch1);
	fail_unless(n->connection.state == NETWORK_CONNECTION_STATE_LOGIN_SENT);
	g_io_channel_set_close_on_unref(ch1, TRUE);
	g_io_channel_unref(ch1);
	disconnect_network(n);
	g_main_iteration(FALSE);
	g_io_channel_read_to_end(ch2, &raw, NULL, &error);
	fail_unless(error == NULL);
	fail_unless(!strcmp(raw, "NICK foo\r\nUSER blah a a :bloeh \r\nQUIT\r\n"), raw);
END_TEST

Suite *network_suite()
{
	Suite *s = suite_create("network");
	TCase *tc_core = tcase_create("core");
	suite_add_tcase(s, tc_core);
	tcase_add_test(tc_core, test_create);
	tcase_add_test(tc_core, test_uncreate);
	tcase_add_test(tc_core, test_login);
	return s;
}
