/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2007 Jelmer Vernooĳ <jelmer@jelmer.uk>

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

#include "libirc/python/irc.h"

extern struct global *my_global;

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

typedef struct {
	PyObject_HEAD
} PyNetworkDictObject;

static Py_ssize_t py_network_dict_length(PyNetworkDictObject *self)
{
	return g_list_length(my_global->networks);
}

static PyObject *py_network_dict_get(PyNetworkDictObject *self, PyObject *py_name)
{
	struct irc_network *n;
	char *name;
	PyNetworkObject *ret;

	if (!PyString_Check(py_name)) {
		PyErr_SetNone(PyExc_KeyError);
		return NULL;
	}

	name = PyString_AsString(py_name);

	n = find_network(my_global->networks, name);
	if (n == NULL) {
		PyErr_SetNone(PyExc_KeyError);
		return NULL;
	}

	ret = PyObject_New(PyNetworkObject, &PyNetworkType);
	if (ret == NULL) {
		PyErr_NoMemory();
		return NULL;
	}
	Py_INCREF(self);
	ret->network = irc_network_ref(n);
	return (PyObject *)ret;
}	

static PyMappingMethods py_network_dict_mapping = {
	.mp_length = (lenfunc)py_network_dict_length,
	.mp_subscript = (binaryfunc)py_network_dict_get,
};

static PyTypeObject PyNetworkDictType = {
	.tp_name = "NetworkDict",
	.tp_basicsize = sizeof(PyNetworkDictObject),
	.tp_dealloc = (destructor)PyObject_Del,
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_as_mapping = &py_network_dict_mapping,
};

static PyObject *PyNetworkDict_New(void)
{
	PyNetworkDictObject *ret = PyObject_New(PyNetworkDictObject, &PyNetworkDictType);
	if (ret == NULL) {
		PyErr_NoMemory();
		return NULL;
	}
	return (PyObject *)ret;
}

void initctrlproxy(void)
{
	PyObject *m, *networkdict;

	if (PyType_Ready(&PyNetworkDictType) < 0)
		return;

	m = Py_InitModule3("ctrlproxy", ctrlproxy_methods,
					   "ControlProxy");
	if (m == NULL)
		return;

	PyModule_AddIntConstant(m, "LOG_DATA", LOG_DATA);
	PyModule_AddIntConstant(m, "LOG_TRACE", LOG_TRACE);
	PyModule_AddIntConstant(m, "LOG_INFO", LOG_INFO);
	PyModule_AddIntConstant(m, "LOG_WARNING", LOG_WARNING);
	PyModule_AddIntConstant(m, "LOG_ERROR", LOG_ERROR);
	networkdict = PyNetworkDict_New();
	if (networkdict == NULL) {
		return;
	}
	PyModule_AddObject(m, "networks", networkdict);
}
