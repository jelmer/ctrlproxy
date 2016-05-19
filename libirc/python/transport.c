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

static int py_transport_dealloc(PyTransportObject *self)
{
    if (self->parent != NULL)
        Py_DECREF(self->parent);
    else
        free_irc_transport(self->transport);
    PyObject_Del((PyObject *)self);
    return 0;
}

static gboolean py_transport_is_connected(void *data)
{
    PyObject *obj = (PyObject *)data, *ret;
    gboolean boolret;

    ret = PyObject_CallMethod(obj, "is_connected", "");
    if (ret == NULL) {
        return FALSE;
    }

    if (!PyBool_Check(ret)) {
        PyErr_SetString(PyExc_TypeError, "is_connected didn't return a boolean");
        Py_DECREF(ret);
        return FALSE;
    }

    boolret = (ret == Py_True);

    Py_DECREF(ret);
    return boolret;
}

static void py_transport_disconnect(void *data)
{
    PyObject *obj = (PyObject *)data, *ret;
    /* Nothing */
    ret = PyObject_CallMethod(obj, "disconnect", "");
    if (ret == NULL) {
        PyErr_Clear();
        return;
    }
    Py_DECREF(ret);
}

static gboolean py_transport_send_line(struct irc_transport *transport, const struct irc_line *l, GError **error)
{
    PyObject *ret;
    PyLineObject *py_line;

    py_line = (PyLineObject *)PyObject_New(PyLineObject, &PyLineType);
    py_line->line = linedup(l);

    ret = PyObject_CallMethod(transport->backend_data, "send_line", "O", py_line);
    Py_DECREF(py_line);

    if (ret == NULL) {
        g_error_set_python(error);
        return FALSE;
    }

    Py_DECREF(ret);

    return TRUE;
}

static char *py_transport_get_peer_name(void *data)
{
    PyObject *obj = (PyObject *)data, *ret;
    char *strret;

    ret = PyObject_Str(obj);
    if (ret == NULL) {
        return NULL;
    }
    if (!PyString_Check(ret)) {
        PyErr_SetNone(PyExc_TypeError);
        Py_DECREF(ret);
        return NULL;
    }
    strret = g_strdup(PyString_AsString(ret));
    Py_DECREF(ret);

    return strret;
}

static void py_transport_activate(struct irc_transport *transport)
{
    PyObject *obj = (PyObject *)transport->backend_data, *ret;

    ret = PyObject_CallMethod(obj, "activate", "");
    Py_XDECREF(ret);
}

static gboolean py_transport_set_charset(struct irc_transport *transport, const char *charsetname)
{
    PyObject *obj = (PyObject *)transport->backend_data, *ret;

    ret = PyObject_CallMethod(obj, "set_charset", "s", charsetname);
    if (ret == NULL) {
        return FALSE;
    }

    Py_DECREF(ret);
    return TRUE;
}


struct irc_transport_ops py_transport_ops = {
    .free_data = (void (*) (void *))Py_DecRef,
    .is_connected = py_transport_is_connected,
    .disconnect = py_transport_disconnect,
    .send_line = py_transport_send_line,
    .get_peer_name = py_transport_get_peer_name,
    .activate = py_transport_activate,
    .set_charset = py_transport_set_charset,
};

static PyObject *py_transport_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    char *kwnames[] = { NULL };
    PyTransportObject *self;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "", kwnames))
        return NULL;

    self = (PyTransportObject *)type->tp_alloc(type, 0);
    if (self == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    self->parent = NULL;
    self->transport = g_new0(struct irc_transport, 1);
    self->transport->backend_data = self;
    self->transport->backend_ops = &py_transport_ops;

    return (PyObject *)self;
}

struct irc_transport *wrap_py_transport(PyObject *obj)
{
    struct irc_transport *transport;

    transport = g_new0(struct irc_transport, 1);
    Py_INCREF(obj);
    transport->backend_data = obj;
    transport->backend_ops = &py_transport_ops;

    return transport;
}

static PyObject *py_transport_inject_line(PyTransportObject *self, PyObject *line)
{
    struct irc_line *l;
    gboolean ret;

    if (self->transport->callbacks == NULL || self->transport->callbacks->recv == NULL) {
        PyErr_SetNone(PyExc_AttributeError);
        return NULL;
    }

    l = PyObject_AsLine(line);
    if (l == NULL) {
        return NULL;
    }

    ret = self->transport->callbacks->recv(self->transport, l);
    free_line(l);

    return PyBool_FromLong(ret);
}

static PyMethodDef py_transport_methods[] = {
    { "inject_line", (PyCFunction)py_transport_inject_line,
        METH_O, "Inject line" },
    { NULL }
};

PyTypeObject PyTransportType = {
    .tp_name = "Transport",
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = py_transport_new,
    .tp_methods = py_transport_methods,
    .tp_basicsize = sizeof(PyTransportObject),
    .tp_dealloc = (destructor)py_transport_dealloc,
};


