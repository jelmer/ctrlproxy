/*    ctrlproxy: A modular IRC proxy
 *    (c) 2002-2009 Jelmer Vernooij <jelmer@jelmer.uk>
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
    char *kwnames[] = { "path", "state", "truncate", NULL };
    PyLinestackObject *self;
	char *data_dir;
	int truncate = FALSE;
	PyObject *py_state;
    struct irc_network_state *state;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "sO|i", kwnames,
                                     &data_dir, &py_state, &truncate))
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
    self->linestack = create_linestack(data_dir, truncate, state);

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

    if (!ret) {
        PyErr_SetNone(PyExc_RuntimeError);
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *py_linestack_replay(PyLinestackObject *self, PyObject *args)
{
    guint64 from, to;
    PyObject *py_state;
    gboolean ret;
    struct irc_network_state *state;
    if (!PyArg_ParseTuple(args, "OLL", &py_state, &from, &to))
        return NULL;

    state = PyObject_AsNetworkState(py_state);
    if (state == NULL) {
        return NULL;
    }

    ret = linestack_replay(self->linestack, &from, &to, state);
    if (ret == FALSE) {
        PyErr_SetNone(PyExc_RuntimeError);
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *py_linestack_get_marker(PyLinestackObject *self)
{
    linestack_marker m = linestack_get_marker(self->linestack);
    if (m == NULL) {
        PyErr_SetNone(PyExc_RuntimeError);
        return NULL;
    }
    return PyLong_FromLong(*m);
}

static PyObject *py_linestack_send(PyLinestackObject *self, PyObject *args)
{
    int time_offset = 0;
    gboolean dataonly = FALSE;
    gboolean timed = FALSE;
    guint64 from, to;
    PyObject *py_client;
    struct irc_client *client;
    gboolean ret;
    char *object = NULL;

    if (!PyArg_ParseTuple(args, "LLO|ziii", &from, &to, &py_client,
                          &object, &dataonly, &timed, &time_offset))
        return NULL;

    client = PyObject_AsClient(py_client);
    if (client == NULL) {
        return NULL;
    }

    if (object != NULL) {
        ret = linestack_send_object(self->linestack, object, &from, &to, client,
                                    dataonly, timed, time_offset);
    } else {
        ret = linestack_send(self->linestack, &from, &to, client, dataonly,
                             timed, time_offset);
    }
    if (!ret) {
        PyErr_SetNone(PyExc_RuntimeError);
        return NULL;
    }

    Py_RETURN_NONE;
}

typedef struct {
    PyObject_HEAD
    PyLinestackObject *parent;
    guint64 from, to;
} PyLinestackIterObject;

static int py_linestack_iter_dealloc(PyLinestackIterObject *self)
{
    Py_DECREF(self->parent);
    PyObject_Del(self);
    return 0;
}

static PyObject *py_linestack_iter_next(PyLinestackIterObject *self)
{
    time_t time;
    struct irc_line *line = NULL;
    gboolean ret;
    PyLineObject *py_line;
    PyObject *py_ret;
    if (self->from == self->to) {
        PyErr_SetNone(PyExc_StopIteration);
        return NULL;
    }

    ret = linestack_read_entry(self->parent->linestack, self->from,
                         &line, &time);
    if (ret == FALSE) {
        PyErr_SetNone(PyExc_RuntimeError);
        return NULL;
    }

    py_line = PyObject_New(PyLineObject, &PyLineType);
    py_line->line = line;

    py_ret = Py_BuildValue("(Nl)", py_line, time);

    self->from++;
    return py_ret;
}

PyTypeObject PyLinestackIterType = {
    .tp_name = "LinestackIter",
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_basicsize = sizeof(PyLinestackIterObject),
    .tp_iter = PyObject_SelfIter,
    .tp_iternext = (iternextfunc)py_linestack_iter_next,
    .tp_dealloc = (destructor)py_linestack_iter_dealloc,
};

static PyObject *py_linestack_traverse(PyLinestackObject *self, PyObject *args)
{
    PyLinestackIterObject *ret;
    ret = PyObject_New(PyLinestackIterObject, &PyLinestackIterType);
    if (ret == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    Py_INCREF(self);
    ret->parent = self;

    if (!PyArg_ParseTuple(args, "LL", &ret->from, &ret->to)) {
        Py_DECREF(ret);
        return NULL;
    }

    return (PyObject *)ret;
}

static PyMethodDef py_linestack_methods[] = {
    { "insert_line", (PyCFunction)py_linestack_insert_line,
        METH_VARARGS, "Insert line" },
    { "replay", (PyCFunction)py_linestack_replay,
        METH_VARARGS, "Replay" },
    { "get_marker", (PyCFunction)py_linestack_get_marker,
        METH_NOARGS, "Get marker" },
    { "send", (PyCFunction)py_linestack_send, METH_VARARGS,
        "Send" },
    { "traverse", (PyCFunction)py_linestack_traverse, METH_VARARGS,
        "Traverse" },
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
