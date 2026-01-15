/*
    ircdtorture: an IRC RFC compliance tester
	(c) 2005 Jelmer Vernooĳ <jelmer@jelmer.uk>

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
#include <string.h>
#include <check.h>
#include "ctrlproxy.h"

START_TEST(test_rfccmp)
{
	fail_if (str_rfc1459cmp("abcde", "ABCDE") != 0);
	fail_if (str_rfc1459cmp("abcde~{}", "ABCDE^[]") != 0);
	fail_if (str_asciicmp("abcde", "ABCDE") != 0);
	fail_if (str_asciicmp("abcde[]", "ABCDE[]") != 0);
	fail_if (str_asciicmp("abcde{}", "ABCDE[]") == 0);
	fail_if (str_strictrfc1459cmp("abcde{}", "ABCDE[]") != 0);
	fail_if (str_strictrfc1459cmp("abcde{}^", "ABCDE[]~") == 0);
	fail_if (str_strictrfc1459cmp("abcde{}", "abcde{}") != 0);
	fail_if (str_strictrfc1459cmp("abcde{}^", "abcde{}") == 0);
	fail_if (str_strictrfc1459cmp("abcde", "abcde{}") == 0);
}
END_TEST

Suite *cmp_suite()
{
	Suite *s = suite_create("cmp");
	TCase *tc_core = tcase_create("core");
	suite_add_tcase(s, tc_core);
	tcase_add_test(tc_core, test_rfccmp);
	return s;
}
