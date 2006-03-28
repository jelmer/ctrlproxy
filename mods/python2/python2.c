/* 
	ctrlproxy: A modular IRC proxy
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

#include <Python.h>
#include "ctrlproxy.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <glib.h>

static gboolean fini_plugin(struct plugin *p)
{
	Py_Finalize();
	return TRUE;
}

static void load_config(struct global *global)
{
	const char *filename;
	GDir *dir; 
	char *mypath = g_build_filename(global->config->config_dir, "scripts/python", NULL);
	char *oldpath, *newpath;

	oldpath = Py_GetPath();

	newpath = g_strdup_printf("%s:%s", mypath, oldpath);

	g_free(mypath);

	PySys_SetPath(newpath);

	g_free(newpath);

	dir = g_dir_open(global->config->config_dir, 0, NULL);
	if (!dir)
		return;

	while ((filename = g_dir_read_name(dir))) {
		log_global("python2", LOG_INFO, "Loading `%s'", filename);

		if (PyImport_ImportModule(g_strdup(filename)) == NULL) {
			PyErr_Print();
			PyErr_Clear();
		}
	}

	g_dir_close(dir);
}

static gboolean init_plugin(void)
{
	Py_Initialize();

	register_config_notify(load_config);
	return TRUE;
}

struct plugin_ops plugin = {
	.name = "python2",
	.version = 0,
	.init = init_plugin,
};
