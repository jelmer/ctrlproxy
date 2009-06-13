/*    ctrlproxy: A modular IRC proxy
 *    (c) 2002-2009 Jelmer Vernooij <jelmer@nl.linux.org>
 *     vim: expandtab
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdbool.h>
#include "ctrlproxy.h"
#include "redirect.h"
#include "libirc/python/irc.h"

static int py_linestack_dealloc(PyLinestackObject *self)
{
    if (self->parent != NULL)
        Py_DECREF(self->parent);
    else
        free_linestack_context(self->linestack);
    PyObject_Del((PyObject *)self);
    return 0;
}

static PyObject *py_linestack_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    char *kwnames[] = { "name", "truncate", "basedir", "state", NULL };
    PyLinestackObject *self;
	char *name, *basedir;
	int truncate;
	PyObject *py_state;
    struct irc_network_state *state;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "sisO", kwnames, 
                                     &name, &truncate, &basedir, &py_state))
        return NULL;

    self = (PyLinestackObject *)type->tp_alloc(type, 0);
    if (self == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    state = PyObject_AsNetworkState(py_state);
    if (state == NULL) {
        return NULL;
    }

    self->parent = NULL;
    self->linestack = create_linestack(name, truncate, basedir, state);

    return (PyObject *)self;
}

static PyObject *py_linestack_insert_line(PyLinestackObject *self, PyObject *args)
{
    struct irc_line *l;
    gboolean ret;
	enum data_direction dir;
	PyObject *py_line, *py_state;
	struct irc_network_state *state;

	if (!PyArg_ParseTuple(args, "OiO", &py_line, &dir, &py_state))
		return NULL;

    l = PyObject_AsLine(py_line);
    if (l == NULL) {
        return NULL;
    }

    state = PyObject_AsNetworkState(py_state);
    if (state == NULL) {
        return NULL;
    }

    ret = linestack_insert_line(self->linestack, l, dir, state);
    free_line(l);

    return PyBool_FromLong(ret);
}

static PyMethodDef py_linestack_methods[] = {
    { "insert_line", (PyCFunction)py_linestack_insert_line, 
        METH_VARARGS, "Insert line" },
    { NULL }
};

PyTypeObject PyLinestackType = {
    .tp_name = "Linestack",
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = py_linestack_new,
    .tp_methods = py_linestack_methods,
    .tp_basicsize = sizeof(PyLinestackObject),
    .tp_dealloc = (destructor)py_linestack_dealloc,
};


