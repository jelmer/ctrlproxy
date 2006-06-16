/*
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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <glib.h>
#include <check.h>
#include "ctrlproxy.h"

START_TEST(test_list_make_string)
	GList *gl = NULL;
	char *ret;

	ret = list_make_string(NULL);
	fail_unless (strcmp(ret, "") == 0);

	gl = g_list_append(gl, "bla");
	gl = g_list_append(gl, "bloe");
	
	ret = list_make_string(gl);
	fail_unless (strcmp(ret, "bla bloe") == 0);
	
END_TEST

Suite *util_suite(void)
{
	Suite *s = suite_create("util");
	TCase *tc_core = tcase_create("Core");
	suite_add_tcase(s, tc_core);
	tcase_add_test(tc_core, test_list_make_string);
	return s;
}
