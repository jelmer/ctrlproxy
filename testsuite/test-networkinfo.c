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
#include <stdbool.h>
#include "internals.h"
#include "torture.h"

START_TEST(test_network_supports)
	struct network_info info;
	info.features = g_hash_table_new(NULL, NULL);
	g_hash_table_insert(info.features, "foo", "bla");
	fail_if(network_supports(&info, "foo") == false);
	fail_if(network_supports(&info, "foobar") == true);
END_TEST

START_TEST(test_is_prefix)
	struct network_info info;
	info.features = g_hash_table_new(NULL, NULL);
	g_hash_table_insert(info.features, "PREFIX", "(qaohv)~&@%+");
	fail_unless(is_prefix('&', &info));
	fail_if(is_prefix(' ', &info));
	g_hash_table_replace(info.features, "PREFIX", "");
	fail_if(is_prefix('&', &info));
END_TEST

START_TEST(test_get_prefix_by_mode)
	struct network_info info;
	info.features = g_hash_table_new(NULL, NULL);
	g_hash_table_insert(info.features, "PREFIX", "(qaohv)~&@%+");
	fail_unless(get_prefix_by_mode('a', &info) == '&');
	fail_unless(get_prefix_by_mode('q', &info) == '~');
	fail_unless(get_prefix_by_mode('h', &info) == '%');
	fail_unless(get_prefix_by_mode('!', &info) == ' ');
	g_hash_table_replace(info.features, "PREFIX", "(qaohv~&@%+");
	fail_unless(get_prefix_by_mode('a', &info) == ' ');
END_TEST

START_TEST(test_get_charset)
	struct network_info info;
	info.features = g_hash_table_new(NULL, NULL);
	g_hash_table_insert(info.features, "CHARSET", "ascii");
	fail_unless(!strcmp(get_charset(&info), "ascii"));
END_TEST

START_TEST(test_get_charset_default)
	struct network_info info;
	info.features = g_hash_table_new(NULL, NULL);
	fail_unless(!strcmp(get_charset(&info), "iso8859-15"));
END_TEST

Suite *networkinfo_suite()
{
	Suite *s = suite_create("networkinfo");
	TCase *tc_core = tcase_create("core");
	suite_add_tcase(s, tc_core);
	tcase_add_test(tc_core, test_network_supports);
	tcase_add_test(tc_core, test_is_prefix);
	tcase_add_test(tc_core, test_get_prefix_by_mode);
	tcase_add_test(tc_core, test_get_charset);
	tcase_add_test(tc_core, test_get_charset_default);
	return s;
}
