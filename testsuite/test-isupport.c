
/*
    ircdtorture: an IRC RFC compliance tester
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

#include <stdio.h>
#include <string.h>
#include <check.h>
#include "ctrlproxy.h"

START_TEST(isupport_mode_is_channelmode)
{
	struct irc_network_info ni = {
		.supported_channel_modes = "aob"
	};
	fail_if (is_channel_mode(&ni, 'c'));
	fail_unless (is_channel_mode(&ni, 'a'));
	fail_unless (is_channel_mode(&ni, 'b'));
	ni.supported_channel_modes = NULL;
	fail_if (is_channel_mode(&ni, 'b'));
}
END_TEST

START_TEST(isupport_isprefix)
{
	struct irc_network_info ni = {
		.prefix = "(ov)@+"
	};
	fail_if (!is_prefix('@', &ni));
	fail_if (is_prefix('a', &ni));
	fail_if (is_prefix(0, &ni));
}
END_TEST

START_TEST(isupport_ischannelname)
{
	struct irc_network_info ni = {
		.chantypes = "#&"
	};
	fail_if (!is_channelname("#bla", &ni));
	fail_if (!is_channelname("&bla", &ni));
	fail_if (is_channelname("bla", &ni));
}
END_TEST

START_TEST(isupport_prefixbymode)
{
	struct irc_network_info ni = {
		.prefix = "(ov)@+"
	};
	fail_if (get_prefix_by_mode('o', &ni) != '@');
	fail_if (get_prefix_by_mode('v', &ni) != '+');
	fail_if (get_prefix_by_mode('x', &ni) != ' ');
}
END_TEST

START_TEST(isupport_is_prefix_mode)
{
	struct irc_network_info ni = {
		.prefix = "(ov)@+"
	};
	fail_unless (is_prefix_mode(&ni, 'o'));
	fail_unless (is_prefix_mode(&ni, 'v'));
	fail_if (is_prefix_mode(&ni, ' '));
}
END_TEST

START_TEST(isupport_modebyprefix)
{
	struct irc_network_info ni = {
		.prefix = "(ov)@+"
	};
	fail_if (get_mode_by_prefix('@', &ni) != 'o');
	fail_if (get_mode_by_prefix('+', &ni) != 'v');
	fail_if (get_mode_by_prefix('%', &ni) != 0);
}
END_TEST

START_TEST(isupport_prefixfrommodes)
{
	struct irc_network_info ni = {
		.prefix = "(ov)@+"
	};
	irc_modes_t modes;
	memset(modes, 0, sizeof(modes)); modes_set_mode(modes, 'o');
	fail_unless (get_prefix_from_modes(&ni, modes) == '@');
	modes_set_mode(modes, 'v');
	fail_unless (get_prefix_from_modes(&ni, modes) == '@');
	modes_unset_mode(modes, 'o');
	fail_unless (get_prefix_from_modes(&ni, modes) == '+');
	modes_set_mode(modes, 'x');
	fail_unless (get_prefix_from_modes(&ni, modes) == '+');
	modes_unset_mode(modes, 'v');
	fail_unless (get_prefix_from_modes(&ni, modes) == 0);
}
END_TEST

START_TEST(isupport_info_parse_casemapping)
{
	struct irc_network_info *info = g_new0(struct irc_network_info, 1);
	network_info_parse(info, "CASEMAPPING=ascii");
	fail_unless (info->casemapping == CASEMAP_ASCII);
	network_info_parse(info, "CASEMAPPING=strict-rfc1459");
	fail_unless (info->casemapping == CASEMAP_STRICT_RFC1459);
}
END_TEST

START_TEST(isupport_info_parse_name)
{
	struct irc_network_info *info = g_new0(struct irc_network_info, 1);
	network_info_parse(info, "NETWORK=bla");
	fail_unless (strcmp(info->name, "bla") == 0);
}
END_TEST

Suite *isupport_suite(void)
{
	Suite *s = suite_create("isupport");
	TCase *tc_core = tcase_create("core");
	suite_add_tcase(s, tc_core);
	tcase_add_test(tc_core, isupport_isprefix);
	tcase_add_test(tc_core, isupport_ischannelname);
	tcase_add_test(tc_core, isupport_prefixbymode);
	tcase_add_test(tc_core, isupport_modebyprefix);
	tcase_add_test(tc_core, isupport_prefixfrommodes);
	tcase_add_test(tc_core, isupport_info_parse_casemapping);
	tcase_add_test(tc_core, isupport_info_parse_name);
	tcase_add_test(tc_core, isupport_mode_is_channelmode);
	tcase_add_test(tc_core, isupport_is_prefix_mode);
	return s;
}
