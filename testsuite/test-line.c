/*
	(c) 2005 Jelmer Vernooĳ <jelmer@jelmer.uk>

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
#include <glib.h>
#include <check.h>
#include "ctrlproxy.h"

START_TEST(test_line_parse_linef)
	struct irc_line *l = irc_parse_linef("data");
	fail_if (l->argc != 1, "Invalid parse");
	fail_if (strcmp(l->args[0], "data") != 0, "Invalid parse");
	fail_if (l->origin != NULL, "Invalid origin");

	l = irc_parse_linef("data arg1");
	fail_if (l->argc != 2, "Invalid parse");
	fail_if (strcmp(l->args[0], "data") != 0, "Invalid parse");
	fail_if (strcmp(l->args[1], "arg1") != 0, "Invalid parse");
	fail_if (l->origin != NULL, "Invalid origin");

	l = irc_parse_linef("data :arg1 ");
	fail_if (l->argc != 2, "Invalid parse");
	fail_if (strcmp(l->args[0], "data") != 0, "Invalid parse");
	fail_if (strcmp(l->args[1], "arg1 ") != 0, "Invalid parse");
	fail_if (l->origin != NULL, "Invalid origin");
END_TEST

START_TEST(test_parse_args)
	struct irc_line *l = irc_parse_line_args("myorigin", "data", NULL);
	fail_if (l->argc != 1, "Invalid parse");
	fail_if (l->origin == NULL, "Invalid origin");
	fail_if (strcmp(l->origin, "myorigin") != 0, "Invalid origin");
	fail_if (strcmp(l->args[0], "data") != 0, "Invalid parse");

	l = irc_parse_line_args("myorigin", "data with space", NULL);
	fail_if (l->argc != 1, "Invalid parse");
	fail_if (l->origin == NULL, "Invalid origin");
	fail_if (strcmp(l->origin, "myorigin") != 0, "Invalid origin");
	fail_if (strcmp(l->args[0], "data with space") != 0, "Invalid parse");

	l = irc_parse_line_args("myorigin", "data", "with", "args", NULL);
	fail_if (l->argc != 3, "Invalid parse");
	fail_if (l->origin == NULL, "Invalid origin");
	fail_if (strcmp(l->origin, "myorigin") != 0, "Invalid origin");
	fail_if (strcmp(l->args[0], "data") != 0, "Invalid parse");
	fail_if (strcmp(l->args[1], "with") != 0, "Invalid parse");
	fail_if (strcmp(l->args[2], "args") != 0, "Invalid parse");

	l = irc_parse_line_args("myorigin", NULL);
	fail_if (l->argc != 0, "Invalid parse");
	fail_if (l->origin == NULL, "Invalid origin");
	fail_if (strcmp(l->origin, "myorigin") != 0, "Invalid origin");

END_TEST

START_TEST(test_prefix_time)
	struct irc_line *l;
	time_t timestamp = 1209309035;
	char stime[512];

	l = irc_parse_line(":me@host PRIVMSG to :hoi\r\n");

	line_prefix_time(l, timestamp);

	strftime(stime, sizeof(stime), "[%H:%M:%S] ", localtime(&timestamp));

	fail_unless (!strncmp(l->args[2], stime, strlen(stime)), "Got %s", l->args[2]);
	fail_unless (!strcmp(l->args[2]+strlen(stime), "hoi"), "Got %s", l->args[2]);

	l = irc_parse_line(":me@host PRIVMSG to :\001ACTION bla\001\r\n");

	line_prefix_time(l, timestamp);

	fail_unless (!strncmp(l->args[2], "\001ACTION ", strlen("\001ACTION ")), "Got: %s", l->args[2]);
	fail_unless (!strncmp(l->args[2]+strlen("\001ACTION "), stime, strlen(stime)), "Got: %s", l->args[2]);
	fail_unless (!strcmp(l->args[2]+strlen("\001ACTION ")+strlen(stime), "bla\001"), "Got: %s", l->args[2]);

	l = irc_parse_line(":me@host PRIVMSG to :\001FINGER bla\001\r\n");

	line_prefix_time(l, timestamp);

	fail_unless (!strcmp(l->args[2], "\001FINGER bla\001"), "Got: %s", l->args[2]);
END_TEST

START_TEST(test_free_null)
	free_line(NULL);
END_TEST

Suite *line_suite(void)
{
	Suite *s = suite_create("line");
	TCase *tc_core = tcase_create("Core");
	suite_add_tcase(s, tc_core);
	tcase_add_test(tc_core, test_line_parse_linef);
	tcase_add_test(tc_core, test_parse_args);
	tcase_add_test(tc_core, test_free_null);
	tcase_add_test(tc_core, test_prefix_time);
	return s;
}
