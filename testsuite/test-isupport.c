
/*
    ircdtorture: an IRC RFC compliancy tester
	(c) 2005 Jelmer Vernooij <jelmer@nl.linux.org>

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

START_TEST(isupport_isprefix)
	struct network_info ni = {
		.prefix = "(ov)@+"
	};
	fail_if (!is_prefix('@', &ni));
	fail_if (is_prefix('a', &ni));
END_TEST

START_TEST(isupport_ischannelname)
	struct network_info ni = {
		.chantypes = "#&"
	};
	fail_if (!is_channelname("#bla", &ni));
	fail_if (!is_channelname("&bla", &ni));
	fail_if (is_channelname("bla", &ni));
END_TEST

START_TEST(isupport_prefixbymode)
	struct network_info ni = {
		.prefix = "(ov)@+"
	};
	fail_if (get_prefix_by_mode('o', &ni) != '@');
	fail_if (get_prefix_by_mode('v', &ni) != '+');
	fail_if (get_prefix_by_mode('x', &ni) != ' ');
END_TEST

START_TEST(isupport_prefixfrommodes)
	struct network_info ni = {
		.prefix = "(ov)@+"
	};
	fail_unless (get_prefix_from_modes(&ni, "o") == '@');
	fail_unless (get_prefix_from_modes(&ni, "ov") == '@');
	fail_unless (get_prefix_from_modes(&ni, "vo") == '@');
	fail_unless (get_prefix_from_modes(&ni, "v") == '+');
	fail_unless (get_prefix_from_modes(&ni, "x") == 0);
END_TEST

START_TEST(isupport_info_parse_casemapping)
	struct network_info *info = g_new0(struct network_info, 1);
	network_info_parse(info, "CASEMAPPING=ascii");
	fail_unless (info->casemapping == CASEMAP_ASCII);
	network_info_parse(info, "CASEMAPPING=strict-rfc1459");
	fail_unless (info->casemapping == CASEMAP_STRICT_RFC1459);
END_TEST

START_TEST(isupport_info_parse_name)
	struct network_info *info = g_new0(struct network_info, 1);
	network_info_parse(info, "NETWORK=bla");
	fail_unless (strcmp(info->name, "bla") == 0);
END_TEST

Suite *isupport_suite(void)
{
	Suite *s = suite_create("isupport");
	TCase *tc_core = tcase_create("core");
	suite_add_tcase(s, tc_core);
	tcase_add_test(tc_core, isupport_isprefix);
	tcase_add_test(tc_core, isupport_ischannelname);
	tcase_add_test(tc_core, isupport_prefixbymode);
	tcase_add_test(tc_core, isupport_prefixfrommodes);
	tcase_add_test(tc_core, isupport_info_parse_casemapping);
	tcase_add_test(tc_core, isupport_info_parse_name);
	return s;
}
