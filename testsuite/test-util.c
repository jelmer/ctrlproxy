/*
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

#include "torture.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <glib.h>
#include "../ctrlproxy.h"

static int test_list_make_string(void)
{
	GList *gl = NULL;
	char *ret;

	ret = list_make_string(NULL);
	if (strcmp(ret, "") != 0)
		return -1;

	gl = g_list_append(gl, "bla");
	gl = g_list_append(gl, "bloe");
	
	ret = list_make_string(gl);
	if (strcmp(ret, "bla bloe") != 0)
		return -2;
	
	return 0;
}

static int test_ctrlproxy_path (void)
{
	char *expected = g_strdup_printf("%s/.ctrlproxy/%s", g_get_home_dir(), "bloe");
	char *result = ctrlproxy_path("bloe");

	if (strcmp(expected, result) != 0)
		return -1;

	g_free(expected);
	g_free(result);

	return 0;
}

void torture_init(void)
{
	register_test("UTIL-LIST-STRING", test_list_make_string);
	register_test("UTIL-CTRLPROXY-PATH", test_ctrlproxy_path);
}
