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
#include "ctrlproxy.h"
#include "torture.h"
#include "nickserv.h"

const char *nickserv_find_nick(struct network *n, char *nick);
const char *nickserv_nick(struct network *n);
void nickserv_identify_me(struct network *network, char *nick);
gboolean nickserv_write_file(GList *nicks, const char *filename);

START_TEST(test_write_file_empty)
	char *fn = torture_tempfile(__FUNCTION__);
	char *contents;
	fail_unless(nickserv_write_file(NULL, fn) == TRUE);
	fail_unless(g_file_get_contents(fn, &contents, NULL, NULL));
	fail_unless(!strcmp(contents, ""), "Excepted empty file, got: %s", contents);
END_TEST

START_TEST(test_write_file_simple_network)
	char *fn = torture_tempfile(__FUNCTION__);
	char *contents;
	struct nickserv_entry n = {
		.network = "bla",
		.nick = "anick",
		.pass = "somepw"
	};
	fail_unless(nickserv_write_file(g_list_append(NULL, &n), fn) == TRUE);
	fail_unless(g_file_get_contents(fn, &contents, NULL, NULL));
	fail_unless(!strcmp(contents, "anick\tsomepw\tbla\n"), "got: %s", contents);
END_TEST


START_TEST(test_write_file_simple_nonetwork)
	char *fn = torture_tempfile(__FUNCTION__);
	char *contents;
	struct nickserv_entry n = {
		.network = NULL,
		.nick = "anick",
		.pass = "somepw"
	};
	fail_unless(nickserv_write_file(g_list_append(NULL, &n), fn) == TRUE);
	fail_unless(g_file_get_contents(fn, &contents, NULL, NULL));
	fail_unless(!strcmp(contents, "anick\tsomepw\t*\n"), "got: %s", contents);
END_TEST

Suite *nickserv_suite()
{
	Suite *s = suite_create("nickserv");
	TCase *tc_core = tcase_create("core");
	suite_add_tcase(s, tc_core);
	tcase_add_test(tc_core, test_write_file_empty);
	tcase_add_test(tc_core, test_write_file_simple_network);
	tcase_add_test(tc_core, test_write_file_simple_nonetwork);
	return s;
}
