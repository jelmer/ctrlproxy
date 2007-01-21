/*
    ircdtorture: an IRC RFC compliancy tester
	(c) 2007 Jelmer Vernooij <jelmer@nl.linux.org>

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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <check.h>
#include "internals.h"
#include "torture.h"

static void test_redirect_response(struct network *n, const char *line)
{
	struct line *l = irc_parse_line(line);	
	redirect_response(n, l);
}

START_TEST(test_465)
	struct network *n = dummy_network();
	test_redirect_response(n, "465 irc.example.com :You are banned");
END_TEST

START_TEST(test_451)
	struct network *n = dummy_network();
	test_redirect_response(n, "451 nick :Not registered");
END_TEST

START_TEST(test_462)
	struct network *n = dummy_network();
	test_redirect_response(n, "462 nick :Already registered");
END_TEST

START_TEST(test_463)
	struct network *n = dummy_network();
	test_redirect_response(n, "463 hostname :Not privileged to connect");
END_TEST

START_TEST(test_464)
	struct network *n = dummy_network();
	test_redirect_response(n, "464 nick :Password mismatch");
END_TEST

START_TEST(test_topic)
	struct network *n = dummy_network();
	test_redirect_response(n, "332 #channel :Foobar");
END_TEST

Suite *redirect_suite()
{
	Suite *s = suite_create("redirect");
	TCase *tc_core = tcase_create("core");
	suite_add_tcase(s, tc_core);
	tcase_add_test(tc_core, test_465);
	tcase_add_test(tc_core, test_451);
	tcase_add_test(tc_core, test_462);
	tcase_add_test(tc_core, test_463);
	tcase_add_test(tc_core, test_464);
	tcase_add_test(tc_core, test_topic);
	return s;
}
