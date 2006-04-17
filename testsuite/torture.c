/*
    torture: test suite for ctrlproxy
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "torture.h"

#include <stdio.h>
#include <glib.h>
#include <gmodule.h>
#include "../ctrlproxy.h"

#define DEFAULT_TIMEOUT 1000

struct torture_test {
	const char *name;
	int (*test) (void);
};

GList *tests = NULL;

void register_test(const char *name, int (*data) (void)) 
{
	struct torture_test *test = g_new0(struct torture_test, 1);
	test->name = g_strdup(name);
	test->test = data;
	tests = g_list_append(tests, test);
}

int run_test(struct torture_test *test)
{
	int ret;
	printf("Running %s... ", test->name);
	fflush(stdout);
	ret = test->test();
	if (ret) printf("failed (%d)!\n", ret); else printf("ok\n");
	return ret;
}

gboolean load_module(const char *name)
{
	GModule *m;
	void (*init_func) (void);
	char *path;

	if (g_file_test(name, G_FILE_TEST_EXISTS))
		path = g_strdup(name);
	else
		path = g_module_build_path(g_get_current_dir(), name);

	m = g_module_open(path, G_MODULE_BIND_LAZY);

	g_free(path);

	if(!m) {
		fprintf(stderr, "Can't open module %s: %s\n", name, g_module_error());
		return FALSE;
	}

	if(!g_module_symbol(m, "torture_init", (gpointer)&init_func)) {
		fprintf(stderr, "Can't find symbol \"torture_init\" in %s: %s\n", name, g_module_error());
		return FALSE;
	}

	init_func();
	return TRUE;
}

int main(int argc, const char *argv[])
{
	GList *gl;
	int ret = 0, i;

	if (argc == 1) {
		fprintf(stderr, "Usage: %s <so-file> [<so-file>...]\n", argv[0]);
		return 1;
	}

	for (i = 1; i < argc; i++) {
		if (!load_module(argv[i]))
			ret = -1;
	}
	
	for (gl = tests; gl; gl = gl->next) {
		if (run_test(gl->data)) 
			ret = -1;
	}

	return ret;
}
