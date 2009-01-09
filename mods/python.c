/* 
	ctrlproxy: A modular IRC proxy
	(c) 2005-2008 Jelmer Vernooij <jelmer@nl.linux.org>

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

#include <Python.h>
#include "ctrlproxy.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <glib.h>

extern void initirc(void);
extern void initctrlproxy(void);

static void load_config(struct global *global)
{
	const char *filename;
	GDir *dir; 
	char *mypath = g_build_filename(global->config->config_dir, "python", NULL);
	char *oldpath, *newpath;

	g_mkdir_with_parents(mypath, 0755);

	oldpath = Py_GetPath();

	newpath = g_strdup_printf("%s:%s", mypath, oldpath);

	PySys_SetPath(newpath);

	g_free(newpath);

	dir = g_dir_open(mypath, 0, NULL);
	if (!dir) {
		log_global(LOG_WARNING, "Unable to open `%s'", mypath);
		g_free(mypath);
		return;
	}
	g_free(mypath);

	while ((filename = g_dir_read_name(dir))) {
		char *modulename;
		if (strcmp(filename + strlen(filename)-3, ".py") != 0)
			continue;

		modulename = g_strndup(filename, strlen(filename)-3);

		log_global(LOG_TRACE, "Loading python plugin `%s'", modulename);

		if (PyImport_ImportModule(modulename) == NULL) {
			PyErr_Print();
			PyErr_Clear();
		}
	}

	g_dir_close(dir);
}

static gboolean init_plugin(void)
{
	Py_Initialize();

	initirc();
	initctrlproxy();

	register_load_config_notify(load_config);
	atexit(Py_Finalize);
	return TRUE;
}

struct plugin_ops plugin = {
	.name = "python",
	.version = CTRLPROXY_PLUGIN_VERSION,
	.init = init_plugin,
};
