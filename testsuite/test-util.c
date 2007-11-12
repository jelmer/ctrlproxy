/*
	(c) 2005-2006 Jelmer Vernooij <jelmer@nl.linux.org>

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
#include <glib.h>
#include <check.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <glib/gstdio.h>
#include "ctrlproxy.h"
#include "torture.h"

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

START_TEST(test_get_description)
	int sock[2];
	GIOChannel *ch;
	char *desc;
	struct sockaddr_in in;
	socklen_t len;

	sock[0] = socket(PF_INET, SOCK_STREAM, 0);
	sock[1] = socket(PF_INET, SOCK_STREAM, 0);

	fail_if(sock[0] < 0);
	fail_if(sock[1] < 0);

	fail_if(listen(sock[1], 1) < 0);

	ch = g_io_channel_unix_new(sock[0]);
	desc = g_io_channel_ip_get_description(ch);
	fail_unless(desc == NULL);
	len = sizeof(in);
	fail_if(getsockname(sock[1], (struct sockaddr *)&in, &len) < 0);
	fail_if(connect(sock[0], (struct sockaddr *)&in, len) < 0);
	desc = g_io_channel_ip_get_description(ch);
	fail_if(desc == NULL);

	g_free(desc);
END_TEST

START_TEST(test_get_set_file_contents)
	char *cont = NULL;
	struct stat st;
	gsize len;
	GError *error = NULL;
	char *f = torture_tempfile("get_set_file_contents");
	fail_unless(rep_g_file_set_contents(f, "bla\nbloe\n", -1, &error) == TRUE, "g_file_set_contents failed: %s", error == NULL?"(null)":error->message);
	fail_unless(g_stat(f, &st) == 0);
	fail_unless((st.st_mode & 0777) == 0644);
	fail_unless(st.st_size == 9);
	fail_unless(rep_g_file_get_contents(f, &cont, &len, &error) == TRUE, "g_file_get_contents failed: %s", error == NULL?"(null)":error->message);
	fail_unless(!strcmp(cont, "bla\nbloe\n"));
	fail_unless(9 == len);
END_TEST

Suite *util_suite(void)
{
	Suite *s = suite_create("util");
	TCase *tc_core = tcase_create("Core");
	suite_add_tcase(s, tc_core);
	tcase_add_test(tc_core, test_get_description);
	tcase_add_test(tc_core, test_list_make_string);
	tcase_add_test(tc_core, test_get_set_file_contents);
	return s;
}
