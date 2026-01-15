/*
	(c) 2005-2008 Jelmer Vernooĳ <jelmer@jelmer.uk>

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
#include <glib.h>
#include <check.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <glib/gstdio.h>
#include "ctrlproxy.h"
#include "torture.h"
#include "internals.h"

#define EXAMPLE_MOTD "This is my\nmotd\nfile\n"

START_TEST(test_read_motd)
{
	char **lines;
	fail_unless(g_file_set_contents("testmotd", EXAMPLE_MOTD, -1, NULL));
	lines = get_motd_lines("testmotd");
	fail_if(strcmp("This is my", lines[0]));
	fail_if(strcmp("motd", lines[1]));
	fail_if(strcmp("file", lines[2]));
	fail_if(NULL, lines[3]);
	unlink("testmotd");
}
END_TEST

Suite *motd_suite(void)
{
	Suite *s = suite_create("motd");
	TCase *tc_core = tcase_create("core");
	suite_add_tcase(s, tc_core);
	tcase_add_test(tc_core, test_read_motd);
	return s;
}
