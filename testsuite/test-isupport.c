
/*
    ircdtorture: an IRC RFC compliancy tester
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
#include <check.h>
#include "ctrlproxy.h"

START_TEST(isupport_isprefix)
	fail_if (!is_prefix('@', NULL));
	fail_if (is_prefix('a', NULL));
END_TEST

START_TEST(isupport_ischannelname)
	fail_if (!is_channelname("#bla", NULL));
	fail_if (!is_channelname("&bla", NULL));
	fail_if (is_channelname("bla", NULL));
END_TEST

START_TEST(isupport_prefixbymode)
	fail_if (get_prefix_by_mode('o',NULL) != '@');
	fail_if (get_prefix_by_mode('v',NULL) != '+');
	fail_if (get_prefix_by_mode('x',NULL) != ' ');
END_TEST

Suite *isupport_suite(void)
{
	Suite *s = suite_create("isupport");
	TCase *tc_core = tcase_create("core");
	suite_add_tcase(s, tc_core);
	tcase_add_test(tc_core, isupport_isprefix);
	tcase_add_test(tc_core, isupport_ischannelname);
	tcase_add_test(tc_core, isupport_prefixbymode);
	return s;
}
