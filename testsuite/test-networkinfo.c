/*
    ircdtorture: an IRC RFC compliancy tester
	(c) 2007 Jelmer Vernooij <jelmer@nl.linux.org>

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
#include <unistd.h>
#include <string.h>
#include <check.h>
#include <stdbool.h>
#include "torture.h"

START_TEST(test_is_prefix)
	struct irc_network_info info;
	memset(&info, 0, sizeof(info));
	network_info_parse(&info, "PREFIX=(qaohv)~&@%+");
	fail_unless(is_prefix('&', &info));
	fail_if(is_prefix(' ', &info));
	network_info_parse(&info, "PREFIX=");
	fail_if(is_prefix('&', &info));
END_TEST

START_TEST(test_get_prefix_by_mode)
	struct irc_network_info info;
	memset(&info, 0, sizeof(info));
	network_info_parse(&info, "PREFIX=(qaohv)~&@%+");
	fail_unless(get_prefix_by_mode('a', &info) == '&');
	fail_unless(get_prefix_by_mode('q', &info) == '~');
	fail_unless(get_prefix_by_mode('h', &info) == '%');
	fail_unless(get_prefix_by_mode('!', &info) == ' ');
	network_info_parse(&info, "PREFIX=(qaohv~&@%+");
	fail_unless(get_prefix_by_mode('a', &info) == '&');
END_TEST

START_TEST(test_get_charset)
	struct irc_network_info info;
	memset(&info, 0, sizeof(info));
	network_info_parse(&info, "CHARSET=ascii");
	fail_unless(!strcmp(get_charset(&info), "ascii"));
END_TEST

START_TEST(test_get_charset_default)
	struct irc_network_info *info;
	info = network_info_init();
	fail_unless(get_charset(info) == NULL);
END_TEST

START_TEST(test_chanmode_type_default)
	fail_unless(network_chanmode_type('I', NULL) == CHANMODE_NICKLIST);
	fail_unless(network_chanmode_type('k', NULL) == CHANMODE_SETTING);
	fail_unless(network_chanmode_type('z', NULL) == CHANMODE_UNKNOWN);
	fail_unless(network_chanmode_type('m', NULL) == CHANMODE_BOOL);
	fail_unless(network_chanmode_type('l', NULL) == CHANMODE_OPT_SETTING);
END_TEST

START_TEST(test_chanmode_type_lookup)
	struct irc_network_info ni;
	memset(&ni, 0, sizeof(ni));
	network_info_parse(&ni, "CHANMODES=b,k,l,ciLmMnOprRst");
	fail_unless(network_chanmode_type('I', &ni) == CHANMODE_UNKNOWN);
	fail_unless(network_chanmode_type('k', &ni) == CHANMODE_SETTING);
	fail_unless(network_chanmode_type('z', &ni) == CHANMODE_UNKNOWN);
	fail_unless(network_chanmode_type('m', &ni) == CHANMODE_BOOL);
	fail_unless(network_chanmode_type('l', &ni) == CHANMODE_OPT_SETTING);
	fail_unless(network_chanmode_type('w', &ni) == CHANMODE_UNKNOWN);
END_TEST

Suite *networkinfo_suite()
{
	Suite *s = suite_create("networkinfo");
	TCase *tc_core = tcase_create("core");
	suite_add_tcase(s, tc_core);
	tcase_add_test(tc_core, test_is_prefix);
	tcase_add_test(tc_core, test_get_prefix_by_mode);
	tcase_add_test(tc_core, test_get_charset);
	tcase_add_test(tc_core, test_get_charset_default);
	tcase_add_test(tc_core, test_chanmode_type_default);
	tcase_add_test(tc_core, test_chanmode_type_lookup);
	return s;
}
