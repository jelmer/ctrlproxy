/*
    ircdtorture: an IRC RFC compliancy tester
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

#include "internals.h"
#include <stdio.h>
#include <string.h>
#include <check.h>

START_TEST(test_create)
	struct global *gl;
	
	gl = init_global();

	fail_if(gl == NULL, "load_global returned NULL");

	free_global(gl);
END_TEST

START_TEST(test_create_nonexisting)
	struct global *gl;
	
	gl = load_global("/some-non-existing/directory", TRUE);

	fail_if(gl != NULL,
			"load_global returned non-NULL for incorrect directory");

	free_global(gl);
END_TEST

START_TEST(test_free_null)
	free_global(NULL);
END_TEST

Suite *user_suite()
{
	Suite *s = suite_create("user");
	TCase *tc_core = tcase_create("core");
	suite_add_tcase(s, tc_core);
	tcase_add_test(tc_core, test_create);
	tcase_add_test(tc_core, test_create_nonexisting);
	tcase_add_test(tc_core, test_free_null);
	return s;
}

