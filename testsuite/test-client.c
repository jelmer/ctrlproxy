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

START_TEST(test_create_no_network)
	GIOChannel *ch = g_io_channel_unix_new(0);
	client_init(NULL, ch, NULL);
END_TEST

START_TEST(test_create_introduction)
	GIOChannel *ch1, *ch2;
	g_io_channel_pair(&ch1, &ch2);
	client_init(NULL, ch1, NULL);
END_TEST

START_TEST(test_disconnect)
	GIOChannel *ch1, *ch2;
	struct client *client;
	char *raw;
	GError *error = NULL;
	gsize length;
	g_io_channel_pair(&ch1, &ch2);
	client = client_init(NULL, ch1, NULL);
	g_io_channel_unref(ch1);
	disconnect_client(client, "Because");
	g_io_channel_read_to_end(ch2, &raw, &length, &error);
	fail_unless(!strcmp(raw, "ERROR :Because\r\n"));
END_TEST

Suite *client_suite()
{
	Suite *s = suite_create("client");
	TCase *tc_core = tcase_create("core");
	suite_add_tcase(s, tc_core);
	tcase_add_test(tc_core, test_create_no_network);
	tcase_add_test(tc_core, test_create_introduction);
	tcase_add_test(tc_core, test_disconnect);
	return s;
}
