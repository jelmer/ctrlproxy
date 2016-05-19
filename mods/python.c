/* 
	ctrlproxy: A modular IRC proxy
	(c) 2005-2008 Jelmer Vernooij <jelmer@jelmer.uk>

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

typedef struct {
	PyObject_HEAD
	admin_handle h;
	char *buffer;
} AdminOutputObject;

static PyObject *py_admin_write(AdminOutputObject *self, PyObject *args)
{
	char *text;
	char **lines;
	int i;
	if (!PyArg_ParseTuple(args, "s", &text))
		return NULL;

	lines = g_strsplit(text, "\n", 0);
	for (i = 0; lines[i+1]; i++) {
		admin_out(self->h, "%s%s", (self->buffer != NULL)?self->buffer:"", lines[i]);
		g_free(self->buffer);
		self->buffer = NULL;
	}

	self->buffer = g_strdup(lines[i]);

	g_strfreev(lines);

	Py_RETURN_NONE;
}

static PyMethodDef py_admin_output_methods[] = {
	{ "write", (PyCFunction)py_admin_write, METH_VARARGS, "Write" },
	{ NULL }
};

static PyTypeObject AdminOutputType = {
	.tp_name = "AdminOutput",
	.tp_dealloc = (destructor)PyObject_Del,
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_basicsize = sizeof(AdminOutputObject),
	.tp_methods = py_admin_output_methods,
};

static void handle_python_admin(admin_handle h, const char * const *args, void *userdata)
{
	char *text;
	PyObject *old_stdout, *old_stderr;
	AdminOutputObject *admin_stdout;

	if (args[1] == NULL) {
		/* Silently ignore */
		return;
	}

	text = g_strjoinv(" ", (char **)args+1);

	old_stdout = PySys_GetObject("stdout");
	old_stderr = PySys_GetObject("stderr");
	admin_stdout = PyObject_New(AdminOutputObject, &AdminOutputType);
	admin_stdout->h = h;
	admin_stdout->buffer = NULL;
	PySys_SetObject("stdout", (PyObject *)admin_stdout);
	PySys_SetObject("stderr", (PyObject *)admin_stdout);
	PyRun_SimpleString(text);
	g_free(text);
	PySys_SetObject("stdout", old_stdout);
	PySys_SetObject("stderr", old_stderr);
	Py_DECREF(admin_stdout);
}

static const struct admin_command python_cmd = {
	.name = "python",
	.handler = handle_python_admin,
};

static gboolean init_plugin(void)
{
	Py_Initialize();

	initirc();
	initctrlproxy();

	if (PyType_Ready(&AdminOutputType) < 0)
		return FALSE;

	register_load_config_notify(load_config);
	register_admin_command(&python_cmd);
	atexit(Py_Finalize);
	return TRUE;
}

struct plugin_ops plugin = {
	.name = "python",
	.version = CTRLPROXY_PLUGIN_VERSION,
	.init = init_plugin,
};
