/*
    ircdtorture: an IRC RFC compliancy tester
	(c) 2008 Jelmer Vernooij <jelmer@nl.linux.org>

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
#include "internals.h"

START_TEST(test_no_subst)
	char *s;
	struct irc_line *l;
	l = irc_parse_line("PRIVMSG :");
	s = custom_subst(NULL, "bla", l, "", FALSE, FALSE);
	fail_if(strcmp(s, "bla"));
	free_line(l);
END_TEST

START_TEST(test_percent)
	char *s;
	struct irc_line *l;
	l = irc_parse_line("PRIVMSG :");
	s = custom_subst(NULL, "bla%%", l, "", FALSE, FALSE);
	fail_if(strcmp(s, "bla%"));
	free_line(l);
END_TEST

START_TEST(test_nick)
	char *s;
	struct irc_line *l;
	l = irc_parse_line(":nick!my@host PRIVMSG :");
	s = custom_subst(NULL, "bla%n%n", l, "", FALSE, FALSE);
	fail_if(strcmp(s, "blanicknick"));
	free_line(l);
END_TEST

Suite *log_subst_suite()
{
	Suite *s = suite_create("log");
	TCase *tc_subst = tcase_create("subst");
	suite_add_tcase(s, tc_subst);
	tcase_add_test(tc_subst, test_no_subst);
	tcase_add_test(tc_subst, test_percent);
	tcase_add_test(tc_subst, test_nick);
	return s;
}
