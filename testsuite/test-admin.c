/*
    ircdtorture: an IRC RFC compliancy tester
	(c) 2006 Jelmer Vernooij <jelmer@nl.linux.org>

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

void init_admin(void);
gboolean process_cmd(admin_handle h, const char *cmd);

static char *data;

static void send_fn (struct admin_handle *ah, const char *new_data)
{
	data = g_strdup_printf("%s%s", data, new_data);
}

static char *run_cmd(const char *cmd)
{
	struct admin_handle ah;
	memset(&ah, 0, sizeof(ah));
	ah.send_fn = send_fn;
	data = g_strdup("");
	process_cmd(&ah, cmd);
	return data;
}

START_TEST(test_echo)
	fail_unless(!strcmp("bla", run_cmd("ECHO bla")), "Expected 'bla', got %s", run_cmd("ECHO bla"));
END_TEST

START_TEST(test_unknown)
	char *result = run_cmd("UNKNOWN bla");
	fail_unless(!strcmp("Can't find command 'UNKNOWN'. Type 'help' for a list of available commands. ", result), "Expected unknown command error, got %s", run_cmd("UNKNOWN bla"));
END_TEST

START_TEST(test_empty)
	char *result = run_cmd("");
	fail_unless(!strcmp("Please specify a command. Use the 'help' command to get a list of available commands", result), "Expected unknown command error, got %s", result);
END_TEST

START_TEST(test_null)
	char *result = run_cmd(NULL);
	fail_unless(!strcmp("Please specify a command. Use the 'help' command to get a list of available commands", result), 
				"Expected command error: %s", result);
END_TEST

Suite *admin_suite()
{
	Suite *s = suite_create("admin");
	TCase *tc_core = tcase_create("core");
	init_admin();
	suite_add_tcase(s, tc_core);
	tcase_add_test(tc_core, test_echo);
	tcase_add_test(tc_core, test_unknown);
	tcase_add_test(tc_core, test_null);
	tcase_add_test(tc_core, test_empty);
	return s;
}
