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
#include "irc.h"

START_TEST(test_parse_url)
{
	char *server, *port;
	gboolean ssl;
	fail_unless(irc_parse_url("irc://foo", &server, &port, &ssl));
	fail_unless(strcmp(server, "foo") == 0);
	fail_unless(strcmp(port, IRC_PORT) == 0);
	fail_unless(ssl == FALSE);

	fail_unless(irc_parse_url("irc://foo/", &server, &port, &ssl));
	fail_unless(strcmp(server, "foo") == 0);
	fail_unless(strcmp(port, IRC_PORT) == 0);
	fail_unless(ssl == FALSE);

	fail_unless(irc_parse_url("ircs://foo", &server, &port, &ssl));
	fail_unless(strcmp(server, "foo") == 0);
	fail_unless(strcmp(port, IRCS_PORT) == 0);
	fail_unless(ssl == TRUE);

	fail_unless(irc_parse_url("ircs://foo:5000", &server, &port, &ssl));
	fail_unless(strcmp(server, "foo") == 0);
	fail_unless(strcmp(port, "5000") == 0);
	fail_unless(ssl == TRUE);

	fail_if(irc_parse_url("http://foo:5000", &server, &port, &ssl));

	fail_unless(irc_parse_url("foo:5000", &server, &port, &ssl));
	fail_unless(strcmp(server, "foo") == 0);
	fail_unless(strcmp(port, "5000") == 0);
	fail_unless(ssl == FALSE);

	fail_unless(irc_parse_url("foo", &server, &port, &ssl));
	fail_unless(strcmp(server, "foo") == 0);
	fail_unless(strcmp(port, IRC_PORT) == 0);
	fail_unless(ssl == FALSE);
}
END_TEST

START_TEST(test_create_url)
{
	fail_unless(strcmp(irc_create_url("foo", IRC_PORT, FALSE), "irc://foo") == 0);
	fail_unless(strcmp(irc_create_url("foo", IRCS_PORT, TRUE), "ircs://foo") == 0);
	fail_unless(strcmp(irc_create_url("foo", "ircd", TRUE), "ircs://foo:ircd") == 0);
	fail_unless(strcmp(irc_create_url("foo", "ircd", TRUE), "ircs://foo:ircd") == 0);
	fail_unless(strcmp(irc_create_url("foo", "ircs", FALSE), "irc://foo:ircs") == 0);
	fail_unless(strcmp(irc_create_url("foo", "6680", FALSE), "irc://foo:6680") == 0);
}
END_TEST

Suite *url_suite()
{
	Suite *s = suite_create("url");
	TCase *tc_core = tcase_create("core");
	suite_add_tcase(s, tc_core);
	tcase_add_test(tc_core, test_parse_url);
	tcase_add_test(tc_core, test_create_url);
	return s;
}
