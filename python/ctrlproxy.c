/* 
	ctrlproxy: A modular IRC proxy
	(c) 2002-2007 Jelmer Vernooij <jelmer@nl.linux.org>

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

#include <Python.h>
#include "ctrlproxy.h"

static PyObject *py_log_global(PyObject *self, PyObject *args)
{
	int level;
	char *text;
	if (!PyArg_ParseTuple(args, "is", &level, &text))
		return NULL;

	log_global(level, "%s", text);

	Py_RETURN_NONE;
}

static PyMethodDef ctrlproxy_methods[] = {
	{ "log_global", (PyCFunction)py_log_global, METH_VARARGS,
		"log_global(level, text)\n"
		"Log" },
	{ NULL }
};

void initctrlproxy(void)
{
	PyObject *m;

	m = Py_InitModule3("ctrlproxy", ctrlproxy_methods, 
					   "ControlProxy");
	if (m == NULL)
		return;

	PyModule_AddIntConstant(m, "LOG_DATA", LOG_DATA);
	PyModule_AddIntConstant(m, "LOG_TRACE", LOG_TRACE);
	PyModule_AddIntConstant(m, "LOG_INFO", LOG_INFO);
	PyModule_AddIntConstant(m, "LOG_WARNING", LOG_WARNING);
	PyModule_AddIntConstant(m, "LOG_ERROR", LOG_ERROR);
}
