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

void g_error_set_python(GError **error)
{
    PyObject *exception_type, *exception_value, *exception_tb;

    /* FIXME */

    PyErr_Fetch(&exception_type, &exception_value, &exception_tb);

    g_set_error_literal(error, g_quark_from_static_string("libirc-python-error"), 1, PyString_AsString(exception_value));
}

static const struct irc_client_callbacks py_client_callbacks;
const char *get_my_hostname() { return NULL; /* FIXME */ }
void log_global(enum log_level ll, const char *fmt, ...) { /* FIXME */}

PyObject *py_line_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    PyObject *arg;
    PyLineObject *self;
    struct irc_line *l;
    char *kwnames[] = { "data", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", kwnames, &arg))
        return NULL;

    l = PyObject_AsLine(arg);
    if (l == NULL)
        return NULL;

    self = (PyLineObject *)type->tp_alloc(type, 0);
    if (self == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    self->line = l;

    return (PyObject *)self;
}

static void py_line_dealloc(PyLineObject *self)
{
    free_line((struct irc_line *)self->line);
}

static PyObject *py_line_str(PyLineObject *self)
{
    char *str;
    PyObject *ret;
    str = irc_line_string(self->line);
    ret = PyString_FromString(str);
    g_free(str);
    return ret;
}

static PyObject *py_line_repr(PyLineObject *self)
{
    char *str;
    PyObject *ret;
    str = irc_line_string(self->line);
    ret = PyString_FromFormat("Line('%s')", str);
    g_free(str);
    return ret;
}

static PyObject *py_line_get_nick(PyLineObject *self)
{
    char *str = line_get_nick(self->line);
    PyObject *ret = PyString_FromString(str);
    g_free(str);
    return ret;
}

static PyMethodDef py_line_methods[] = {
    { "get_nick", (PyCFunction)py_line_get_nick, METH_NOARGS, 
        "Obtain the nick of the user that sent this line." },
    { NULL }
};

static Py_ssize_t py_line_length(PyLineObject *self)
{
    return self->line->argc;
}

static PyObject *py_line_item(PyLineObject *self, Py_ssize_t i)
{
    if (i >= self->line->argc) {
        PyErr_SetString(PyExc_KeyError, "No such element");
        return NULL;
    }
    return PyString_FromString(self->line->args[i]);
}

static PySequenceMethods py_line_sequence = {
    .sq_length = (lenfunc)py_line_length,
    .sq_item = (ssizeargfunc)py_line_item,
};

static PyObject *py_line_get_origin(PyLineObject *self, void *closure)
{
    if (self->line->origin != NULL) {
        return PyString_FromString(self->line->origin);
    } else {
        Py_RETURN_NONE;
    }
}

static PyGetSetDef py_line_getsetters[] = {
    { "origin", (getter)py_line_get_origin, NULL, "Sender of line" },
    { NULL }
};

static int py_line_cmp(PyLineObject *a, PyLineObject *b)
{
    return irc_line_cmp(a->line, b->line);
}

PyTypeObject PyLineType = {
    .tp_name = "Line",
    .tp_new = py_line_new,
    .tp_dealloc = (destructor)py_line_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_basicsize = sizeof(PyLineObject),
    .tp_str = (reprfunc)py_line_str,
    .tp_repr = (reprfunc)py_line_repr,
    .tp_methods = py_line_methods,
    .tp_doc = "A RFC2459-compatible line.",
    .tp_getset = py_line_getsetters,
    .tp_as_sequence = &py_line_sequence,
    .tp_compare = (cmpfunc)py_line_cmp,
};

struct irc_line *PyObject_AsLine(PyObject *obj)
{
    if (PyString_Check(obj))
        return irc_parse_line(PyString_AsString(obj));

    if (PyObject_TypeCheck(obj, &PyLineType))
        return linedup(((PyLineObject *)obj)->line);
    
    if (PyList_Check(obj)) {
        struct irc_line *l = g_new0(struct irc_line, 1);
        int offset = 0;
        int i;
        if (PyList_Size(obj) > 0) {
            PyObject *item0 = PyList_GetItem(obj, offset);
            if (PyString_Check(item0) && PyString_AsString(item0)[0] == ':') {
                l->origin = g_strdup(PyString_AsString(item0)+1);
                offset++;
            }
        }
        l->argc = PyList_Size(obj) - offset;
        l->args = g_new0(char *, l->argc + 1);
        for (i = 0; i < l->argc; i++) {
            PyObject *item = PyList_GetItem(obj, offset+i);
            if (!PyString_Check(item)) {
                PyErr_SetNone(PyExc_TypeError);
                free_line(l);
                return NULL;
            }
            l->args[i] = g_strdup(PyString_AsString(item));
        }
        l->args[i] = NULL;
        return l;
    }   

    PyErr_SetString(PyExc_TypeError, "Expected line");
    return NULL;
}

static int py_g_list_iter_dealloc(PyGListIterObject *self)
{
    Py_XDECREF(self->parent);
    PyObject_Del(self);
    return 0;
}

static PyObject *py_g_list_iter_next(PyGListIterObject *self)
{
    PyObject *ret;

    if (self->iter == NULL) {
        PyErr_SetNone(PyExc_StopIteration);
        return NULL;
    }
    
    ret = self->converter(self->parent, self->iter->data);

    self->iter = g_list_next(self->iter);

    return ret;
}

static PyTypeObject PyGListIterType = {
    .tp_name = "GListIter",
    .tp_dealloc = (destructor)py_g_list_iter_dealloc,
    .tp_iter = PyObject_SelfIter,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_basicsize = sizeof(PyGListIterObject),
    .tp_iternext = (iternextfunc)py_g_list_iter_next,
};

PyObject *py_g_list_iter(GList *list, PyObject *parent, PyObject *(*converter) (PyObject *parent, void *))
{
    PyGListIterObject *ret = PyObject_New(PyGListIterObject, &PyGListIterType);

    if (ret == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    ret->iter = list;
    ret->converter = converter;
    Py_INCREF(parent);
    ret->parent = parent;
    return (PyObject *)ret;
}   

typedef struct {
    PyObject_HEAD
    struct irc_client *client;
} PyClientObject;

static PyObject *py_client_set_charset(PyClientObject *self, PyObject *args)
{
    bool ret;
    char *name;

    if (!PyArg_ParseTuple(args, "s", &name))
        return NULL;

    ret = client_set_charset(self->client, name);
    if (!ret) {
        PyErr_SetString(PyExc_RuntimeError, "Unable to set character set");
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *py_client_send_line(PyClientObject *self, PyObject *args)
{
    PyObject *py_line;
    struct irc_line *l;
    GError *error = NULL;
    if (!PyArg_ParseTuple(args, "O", &py_line))
        return NULL;

    l = PyObject_AsLine(py_line);
    if (l == NULL)
        return NULL;

    if (!client_send_line(self->client, l, &error)) {
        PyErr_Format(PyExc_RuntimeError, "Error while sending line: %s", error->message);
        g_error_free(error);
        free_line(l);
        return NULL;
    }
    free_line(l);

    Py_RETURN_NONE;
}

static PyObject *py_client_send_state(PyClientObject *self, PyNetworkStateObject *state)
{
    if (!PyObject_TypeCheck(state, &PyNetworkStateType)) {
        PyErr_SetNone(PyExc_TypeError);
        return NULL;
    }

    client_send_state(self->client, state->state);

    Py_RETURN_NONE;
}

static PyObject *py_client_send_state_diff(PyClientObject *self, PyObject *args)
{
    PyNetworkStateObject *state1, *state2;

    if (!PyArg_ParseTuple(args, "OO", &state1, &state2))
        return NULL;

    if (!PyObject_TypeCheck(state1, &PyNetworkStateType) || 
        !PyObject_TypeCheck(state2, &PyNetworkStateType)) {
        PyErr_SetNone(PyExc_TypeError);
        return NULL;
    }

    client_send_state_diff(self->client, state1->state, state2->state);

    Py_RETURN_NONE;
}

static PyObject *py_client_send_motd(PyClientObject *self, PyObject *py_motd)
{
    char **motd;
    int i;
    if (!PyList_Check(py_motd)) {
        PyErr_SetNone(PyExc_TypeError);
        return NULL;
    }

    motd = g_new0(char *, PyList_Size(py_motd) + 1);

    for (i = 0; i < PyList_Size(py_motd); i++) {
        PyObject *item = PyList_GetItem(py_motd, i);
        if (!PyString_Check(item)) {
            PyErr_SetNone(PyExc_TypeError);
            g_free(motd);
            return NULL;
        }

        motd[i] = PyString_AsString(item);
    }
    motd[i] = NULL;

    client_send_motd(self->client, motd);

    g_free(motd);

    Py_RETURN_NONE;
}

static PyObject *py_client_send_channel_mode(PyClientObject *self, PyChannelStateObject *py_channel)
{
    if (!PyObject_TypeCheck(py_channel, &PyChannelStateType)) {
        PyErr_SetNone(PyExc_TypeError);
        return NULL;
    }

    client_send_channel_mode(self->client, py_channel->state);

    Py_RETURN_NONE;
}


static PyObject *py_client_send_topic(PyClientObject *self, PyObject *args)
{
    PyChannelStateObject *py_channel;
    int explicit = FALSE;  

    if (!PyArg_ParseTuple(args, "O|i", &py_channel, &explicit))
        return NULL;

    if (!PyObject_TypeCheck(py_channel, &PyChannelStateType)) {
        PyErr_SetNone(PyExc_TypeError);
        return NULL;
    }

    client_send_topic(self->client, py_channel->state, explicit);

    Py_RETURN_NONE;
}

static PyObject *py_client_send_banlist(PyClientObject *self, PyChannelStateObject *py_channel)
{
    if (!PyObject_TypeCheck(py_channel, &PyChannelStateType)) {
        PyErr_SetNone(PyExc_TypeError);
        return NULL;
    }

    client_send_banlist(self->client, py_channel->state);

    Py_RETURN_NONE;
}

static PyObject *py_client_send_channel_state(PyClientObject *self, PyChannelStateObject *py_channel)
{
    if (!PyObject_TypeCheck(py_channel, &PyChannelStateType)) {
        PyErr_SetNone(PyExc_TypeError);
        return NULL;
    }

    client_send_channel_state(self->client, py_channel->state);

    Py_RETURN_NONE;
}

static PyObject *py_client_send_channel_state_diff(PyClientObject *self, PyObject *args)
{
    PyChannelStateObject *py_channel1, *py_channel2;

    if (!PyArg_ParseTuple(args, "OO", &py_channel1, &py_channel2))
        return NULL;

    if (!PyObject_TypeCheck(py_channel1, &PyChannelStateType) || 
        !PyObject_TypeCheck(py_channel2, &PyChannelStateType)) {
        PyErr_SetNone(PyExc_TypeError);
        return NULL;
    }

    client_send_channel_state_diff(self->client, py_channel1->state,
                                   py_channel2->state);

    Py_RETURN_NONE;
}



static PyObject *py_client_send_nameslist(PyClientObject *self, PyChannelStateObject *py_channel)
{
    if (!PyObject_TypeCheck(py_channel, &PyChannelStateType)) {
        PyErr_SetNone(PyExc_TypeError);
        return NULL;
    }

    client_send_nameslist(self->client, py_channel->state);

    Py_RETURN_NONE;
}



static PyObject *py_client_send_luserchannels(PyClientObject *self, PyObject *arg)
{
    if (!PyInt_Check(arg)) {
        PyErr_SetNone(PyExc_TypeError);
        return NULL;
    }

    client_send_luserchannels(self->client, PyInt_AsLong(arg));

    Py_RETURN_NONE;
}

static PyObject *py_client_send_netsplit(PyClientObject *self, PyObject *args)
{
    char *own_name, *other_name;

    if (!PyArg_ParseTuple(args, "ss", &own_name, &other_name))
        return NULL;

    client_send_netsplit(self->client, own_name, other_name);

    Py_RETURN_NONE;
}

static PyObject *py_client_inject_line(PyClientObject *self, PyObject *line)
{
    struct irc_line *l;
    gboolean ret;

    if (self->client->transport == NULL || self->client->transport->callbacks == NULL || self->client->transport->callbacks->recv == NULL) {
        PyErr_SetNone(PyExc_AttributeError);
        return NULL;
    }

    l = PyObject_AsLine(line);
    if (l == NULL) {
        return NULL;
    }

    ret = self->client->transport->callbacks->recv(self->client->transport, l);
    free_line(l);

    return PyBool_FromLong(ret);
}

static PyObject *py_client_welcome(PyClientObject *self)
{
    Py_RETURN_NONE;
}

static PyMethodDef py_client_methods[] = {
    { "set_charset", (PyCFunction)py_client_set_charset, 
        METH_VARARGS,
        "Change the character set."  
        ":note: None will disable character conversion."
    },
    { "send_line", (PyCFunction)py_client_send_line,
        METH_VARARGS,
        "Send a line to this client." },
    { "send_state", (PyCFunction)py_client_send_state,
        METH_O,
        "Send a network state to a client." },
    { "send_topic", (PyCFunction)py_client_send_topic,
        METH_VARARGS,
        "Send the topic of a channel to the client." },
    { "send_state_diff", (PyCFunction)py_client_send_state_diff,
        METH_VARARGS,
        "Send the diff between two states to a client." },
    { "send_motd", (PyCFunction)py_client_send_motd,
        METH_O,
        "Send a MOTD to a client." },
    { "send_channel_mode", (PyCFunction)py_client_send_channel_mode,
        METH_O,
        "Send the mode for a channel to a client." },
    { "send_banlist", (PyCFunction)py_client_send_banlist,
        METH_O,
        "Send the banlist for a channel to a client." },
    { "send_channel_state", (PyCFunction)py_client_send_channel_state,
        METH_O,
        "Send the state of a channel to a client." },
    { "send_channel_state_diff", (PyCFunction)py_client_send_channel_state_diff,
        METH_VARARGS,
        "Send the diff between two channel states to a client." },
    { "send_nameslist", (PyCFunction)py_client_send_nameslist,
        METH_O,
        "Send the names for a channel to a client." },
    { "send_luserchannels", (PyCFunction)py_client_send_luserchannels,
        METH_O,
        "Send number of user channels to client." },
    { "send_netsplit", (PyCFunction)py_client_send_netsplit,
        METH_VARARGS, "Send netsplit to a client." },
    { "inject_line", (PyCFunction)py_client_inject_line,
        METH_O, "Inject a line." },
    { "welcome", (PyCFunction)py_client_welcome,
        METH_NOARGS, "Dummy function, meant to be overridden" },
    { NULL }
};

static PyObject *py_client_get_state(PyClientObject *self, void *closure)
{
    PyNetworkStateObject *ret;

    if (self->client->state == NULL)
        Py_RETURN_NONE;
    
    ret = PyObject_New(PyNetworkStateObject, &PyNetworkStateType);
    if (ret == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    Py_INCREF(self);
    ret->parent = (PyObject *)self;
    ret->state = self->client->state;

    return (PyObject *)ret;
}

static PyObject *py_client_get_default_target(PyClientObject *self, void *closure)
{
    return PyString_FromString(client_get_default_target(self->client));
}

static PyObject *py_client_get_own_hostmask(PyClientObject *self, void *closure)
{
    const char *hostmask = client_get_own_hostmask(self->client);
    if (hostmask == NULL)
        Py_RETURN_NONE;
    return PyString_FromString(hostmask);
}

static PyObject *py_client_get_default_origin(PyClientObject *self, void *closure)
{
    return PyString_FromString(self->client->default_origin);
}

static PyObject *py_client_get_last_ping(PyClientObject *self, void *closure)
{
    return PyLong_FromLong(self->client->last_ping);
}

static PyObject *py_client_get_last_pong(PyClientObject *self, void *closure)
{
    return PyLong_FromLong(self->client->last_pong);
}

static PyObject *py_client_get_description(PyClientObject *self, void *closure)
{
    return PyString_FromString(self->client->description);
}

static PyObject *py_client_get_authenticated(PyClientObject *self, void *closure)
{
    return PyBool_FromLong(self->client->authenticated);
}

static PyObject *py_client_get_transport(PyClientObject *self, void *closure)
{
    if (self->client->transport == NULL)
        Py_RETURN_NONE;

    if (self->client->transport->backend_ops == &py_transport_ops) {
        PyObject *ret = (PyObject *)self->client->transport->backend_data;
        Py_INCREF(ret);
        return ret;
    } else {
        PyTransportObject *ret;
        ret = PyObject_New(PyTransportObject, &PyTransportType);
        if (ret == NULL) {
            PyErr_NoMemory();
            return NULL;
        }

        Py_INCREF(self);
        ret->parent = (PyObject *)self;
        ret->transport = self->client->transport;
        return (PyObject *)ret;
    }
}

static PyGetSetDef py_client_getsetters[] = {
    { "state", (getter)py_client_get_state, NULL, "State" },
    { "default_target", (getter)py_client_get_default_target, NULL, "Default target" },
    { "own_hostmask", (getter)py_client_get_own_hostmask, NULL, "Own hostmask" },
    { "default_origin", (getter)py_client_get_default_origin, NULL,
        "Default origin that is used to send lines to this client." },
    { "last_ping", (getter)py_client_get_last_ping, NULL,
        "Timestamp of the last moment this client pinged the server." },
    { "last_pong", (getter)py_client_get_last_pong, NULL,
        "Timestamp of the last pong that was received." },
    { "description", (getter)py_client_get_description, NULL,
        "Description of the client" },
    { "authenticated", (getter)py_client_get_authenticated, NULL,
        "Authenticated" },
    { "transport", (getter)py_client_get_transport, NULL,
        "Transport" },
    { NULL }
};

static PyObject *py_client_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    PyClientObject *ret;
    PyObject *py_transport;
    struct irc_transport *transport;
    char *default_origin, *desc;
    char *kwnames[] = { "transport", "default_origin", "desc", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "Oss", kwnames, &py_transport, &default_origin, &desc))
        return NULL;
    
    ret = (PyClientObject *)type->tp_alloc(type, 0);
    if (ret == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    transport = wrap_py_transport(py_transport);

    ret->client = irc_client_new(transport, default_origin, desc, &py_client_callbacks);
    ret->client->private_data = ret;

    return (PyObject *)ret;
}

PyTypeObject PyClientType = {
    .tp_name = "Client",
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = py_client_new,
    .tp_methods = py_client_methods,
    .tp_getset = py_client_getsetters,
    .tp_basicsize = sizeof(PyClientObject)
};

struct irc_client *PyObject_AsClient(PyObject *obj)
{
    if (!PyObject_TypeCheck(obj, &PyClientType)) {
        PyErr_SetNone(PyExc_TypeError);
        return NULL;
    }

    return ((PyClientObject *)obj)->client;
}

static PyObject *py_network_connect(PyNetworkObject *self)
{
    if (!connect_network(self->network)) {
        PyErr_SetString(PyExc_RuntimeError, "Unable to connect to network");
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *py_network_disconnect(PyNetworkObject *self)
{
    disconnect_network(self->network);

    Py_RETURN_NONE;
}
    
static PyObject *py_network_send_line(PyNetworkObject *self, PyObject *args)
{
    PyObject *py_line, *py_client = Py_None;
    struct irc_line *l;

    if (!PyArg_ParseTuple(args, "O|O", &py_line, &py_client))
        return NULL;

    l = PyObject_AsLine(py_line);
    if (l == NULL)
        return NULL;

    if (!network_send_line(self->network, NULL, l)) {
        PyErr_SetString(PyExc_RuntimeError, "Error sending line to network");
        free_line(l);
        return NULL;
    }
    free_line(l);
    Py_RETURN_NONE;
}

static PyObject *py_network_set_charset(PyNetworkObject *self, PyObject *args)
{
    char *name;
    if (!PyArg_ParseTuple(args, "s", &name))
        return NULL;

    if (!network_set_charset(self->network, name)) {
        PyErr_SetString(PyExc_RuntimeError, "Unable to set character set");
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *py_network_next_server(PyNetworkObject *self)
{
        irc_network_select_next_server(self->network);
        Py_RETURN_NONE;
}

static PyObject *py_network_feature_string(PyNetworkObject *self)
{
        char *str = network_generate_feature_string(self->network);
        PyObject *ret = PyString_FromString(str);
        g_free(str);
        return ret;
}

static PyMethodDef py_network_methods[] = {
    { "connect", (PyCFunction)py_network_connect, METH_NOARGS,
        NULL },
    { "disconnect", (PyCFunction)py_network_disconnect, METH_NOARGS,
        NULL },
    { "send_line", (PyCFunction)py_network_send_line, METH_VARARGS,
        NULL },
    { "set_charset", (PyCFunction)py_network_set_charset, METH_VARARGS,
        "Change the character set used to communicate with the network." },
    { "next_server", (PyCFunction)py_network_next_server, METH_NOARGS,
        "Switch to the next server in the list." },
    { "feature_string", (PyCFunction)py_network_feature_string, METH_NOARGS,
        "Obtain the feature list for this network." },
    { NULL }
};

static void py_network_dealloc(PyNetworkObject *self)
{
    irc_network_unref(self->network);
    PyObject_Del(self);
}

static PyObject *py_network_repr(PyNetworkObject *self)
{
    return PyString_FromFormat("<Network '%s'>", self->network->name);
}

static PyObject *py_network_get_name(PyNetworkObject *self, void *closure)
{
    return PyString_FromString(self->network->name);
}

static PyObject *py_network_get_reconnect_interval(PyNetworkObject *self, void *closure)
{
    return PyInt_FromLong(self->network->reconnect_interval);
}   

static int py_network_set_reconnect_interval(PyNetworkObject *self, PyObject *value, void *closure)
{
    if (!PyInt_Check(value)) {
        PyErr_SetNone(PyExc_TypeError);
        return -1;
    }

    self->network->reconnect_interval = PyInt_AsLong(value);
    return 0;
}

static PyObject *py_network_get_info(PyNetworkObject *self, void *closure)
{
    PyNetworkInfoObject *pyinfo = PyObject_New(PyNetworkInfoObject, &PyNetworkInfoType);

    if (pyinfo == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    Py_INCREF(self);
    pyinfo->parent = (PyObject *)self;
    pyinfo->info = self->network->info;

    return (PyObject *)pyinfo;
}

static PyObject *py_network_get_internal_state(PyNetworkObject *self, void *closure)
{
    PyNetworkStateObject *ret;

    if (self->network->internal_state == NULL)
        Py_RETURN_NONE;
    
    ret = PyObject_New(PyNetworkStateObject, &PyNetworkStateType);
    if (ret == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    Py_INCREF(self);
    ret->parent = (PyObject *)self;
    ret->state = self->network->internal_state;

    return (PyObject *)ret;
}

static PyObject *py_network_get_external_state(PyNetworkObject *self, void *closure)
{
    PyNetworkStateObject *ret;

    if (self->network->external_state == NULL)
        Py_RETURN_NONE;
    
    ret = PyObject_New(PyNetworkStateObject, &PyNetworkStateType);
    if (ret == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    Py_INCREF(self);
    ret->parent = (PyObject *)self;
    ret->state = self->network->external_state;

    return (PyObject *)ret;
}

static PyObject *py_network_get_linestack_errors(PyNetworkObject *self, void *closure)
{
    return PyInt_FromLong(self->network->linestack_errors);
}

static PyObject *PyClientFromPtr(struct irc_client *c)
{
    PyClientObject *ret = PyObject_New(PyClientObject, &PyClientType);
    if (ret == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    
    ret->client = c;
    client_ref(c);
    return (PyObject *)ret;
}

static void *PyPtrFromClient(PyObject *obj)
{
    if (!PyObject_TypeCheck(obj, &PyClientType)) {
        PyErr_SetNone(PyExc_TypeError);
        return NULL;
    }

    return ((PyClientObject *)obj)->client;
}   

static PyObject *py_network_get_query_stack(PyNetworkObject *self, void *closure)
{
    PyQueryStackObject *ret = PyObject_New(PyQueryStackObject, &PyQueryStackType);
    if (ret == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    ret->stack = self->network->queries;
    ret->import_userdata = (PyObject *(*)(void *))PyClientFromPtr;
    ret->export_userdata = PyPtrFromClient;
    Py_INCREF(self);
    ret->parent = (PyObject *)self;

    return (PyObject *)ret;
}

static PyGetSetDef py_network_getsetters[] = {
    { "name", (getter)py_network_get_name, NULL, "Name of the network" },
    { "reconnect_interval", (getter)py_network_get_reconnect_interval, (setter)py_network_set_reconnect_interval, "Reconnect interval" },
    { "info", (getter)py_network_get_info, NULL, "Info" },
    { "internal_state", (getter) py_network_get_internal_state, NULL, "Internal state" },
    { "external_state", (getter) py_network_get_external_state, NULL, "External state" },
    { "linestack_errors", (getter) py_network_get_linestack_errors, NULL, "Number of linestack errors that has occurred so far" },
    { "query_stack", (getter)py_network_get_query_stack, NULL, "Query stack" },
    { NULL }
};

PyTypeObject PyNetworkType = {
    .tp_name = "Network",
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_methods = py_network_methods,
    .tp_getset = py_network_getsetters,
    .tp_repr = (reprfunc)py_network_repr,
    .tp_basicsize = sizeof(PyNetworkObject),
    .tp_dealloc = (destructor)py_network_dealloc,
};

static int py_process_from_client(struct irc_client *client, const struct irc_line *line)
{
    PyObject *self, *ret;
    PyLineObject *py_line;
    self = client->private_data;
    py_line = PyObject_New(PyLineObject, &PyLineType);
    py_line->line = linedup(line);
    ret = PyObject_CallMethod(self, "process_from_client", "O", py_line);
    Py_DECREF(py_line);
    Py_XDECREF(ret);
    return 0;
}

static void py_log_client(enum log_level level, const struct irc_client *client, const char *text)
{
    PyObject *self, *ret;
    self = client->private_data;
    ret = PyObject_CallMethod(self, "log", "is", level, text);
    if (ret == NULL) {
        PyErr_Clear(); /* FIXME */
        return;
    }
    Py_DECREF(ret);
}

static int py_process_to_client(struct irc_client *client, const struct irc_line *line)
{
    PyObject *self, *ret;
    PyLineObject *py_line;

    self = client->private_data;
    py_line = PyObject_New(PyLineObject, &PyLineType);
    py_line->line = linedup(line);

    ret = PyObject_CallMethod(self, "process_to_client", "O", py_line);
    Py_DECREF(py_line);
    if (ret == NULL) {
        PyErr_Clear(); /* FIXME: */
        return -1;
    }
    Py_DECREF(ret);

    return 0;
}

static int py_welcome_client(struct irc_client *client)
{
    PyObject *ret, *self;
    self = client->private_data;
    ret = PyObject_CallMethod(self, "welcome", "");
    if (ret == NULL)
        return false;
    if (ret == Py_True || ret == Py_None)
        return true;
    return false;
}

static const struct irc_client_callbacks py_client_callbacks = {
    .log_fn = py_log_client,
    .process_from_client = py_process_from_client,
    .process_to_client = py_process_to_client,
    .welcome = py_welcome_client
};

static PyObject *py_query_stack_record(PyQueryStackObject *self, PyObject *args)
{
    PyObject *py_token, *py_line;
    void *userdata;
    struct irc_line *line;

    if (!PyArg_ParseTuple(args, "OO", &py_token, &py_line))
        return NULL;

    line = PyObject_AsLine(py_line);
    if (line == NULL)
        return NULL;

    if (self->export_userdata == NULL) {
        userdata = py_token;
    } else {
        userdata = self->export_userdata(py_token);
        if (userdata == NULL) {
            free_line(line);
            return NULL;
        }
    }
    return PyBool_FromLong(query_stack_record(self->stack, userdata, line));
}

static PyObject *py_query_stack_redirect(PyQueryStackObject *self, PyObject *args)
{
    PyObject *py_line;
    struct irc_line *line;
    PyObject *ret;

    if (!PyArg_ParseTuple(args, "O", &py_line))
        return NULL;

    line = PyObject_AsLine(py_line);
    if (line == NULL)
        return NULL;

    ret = (PyObject *)query_stack_match_response(self->stack, line);
    if (ret == NULL) {
        Py_RETURN_NONE;
    } else {
        Py_INCREF(ret);
        return ret;
    }
}

static PyObject *py_query_stack_clear(PyQueryStackObject *self)
{
    query_stack_clear(self->stack);
    Py_RETURN_NONE;
}

static PyMethodDef py_query_stack_methods[] = {
    { "record", (PyCFunction)py_query_stack_record, METH_VARARGS, NULL },
    { "response", (PyCFunction)py_query_stack_redirect, METH_VARARGS, NULL },
    { "clear", (PyCFunction)py_query_stack_clear, METH_NOARGS, NULL },
    { NULL }
};

static int py_query_stack_dealloc(PyQueryStackObject *self)
{
    if (self->parent != NULL) {
        Py_DECREF(self->parent);
    } else {
        query_stack_free(self->stack);
    }
    PyObject_Del(self);
    return 0;
}

static PyObject *Py_Id(void *obj)
{
    return obj;
}

static PyObject *py_query_stack_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    PyQueryStackObject *self = (PyQueryStackObject *)type->tp_alloc(type, 0);
    if (self == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    self->stack = new_query_stack((void (*)(void *))Py_IncRef, (void (*)(void *))Py_DecRef);
    self->import_userdata = Py_Id;
    self->export_userdata = NULL;
    self->parent = NULL;

    return (PyObject *)self;
}

static PyObject *py_query_stack_entry_from_ptr(PyQueryStackObject *parent, struct query_stack_entry *e)
{
    return Py_BuildValue("(O&sl)", parent->import_userdata, e->userdata, e->query->name, e->time);
}

static PyObject *py_query_stack_iter(PyQueryStackObject *self)
{
    return py_g_list_iter(self->stack->entries, (PyObject *)self, (PyObject *(*)(PyObject *, void*))py_query_stack_entry_from_ptr);
}

static Py_ssize_t py_query_stack_len(PyQueryStackObject *self)
{
    return g_list_length(self->stack->entries);
}

static PySequenceMethods py_query_stack_sequence = {
    .sq_length = (lenfunc)py_query_stack_len,
};

PyTypeObject PyQueryStackType = {
    .tp_name = "QueryStack",
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = py_query_stack_new,
    .tp_methods = py_query_stack_methods,
    .tp_iter = (getiterfunc)py_query_stack_iter,
    .tp_basicsize = sizeof(PyQueryStackObject),
    .tp_dealloc = (destructor)py_query_stack_dealloc,
    .tp_as_sequence = &py_query_stack_sequence,
};


static PyMethodDef irc_methods[] = { 
    { NULL }
};

void initirc(void)
{
    PyObject *m;

    if (PyType_Ready(&PyLineType) < 0)
        return;

    if (PyType_Ready(&PyClientType) < 0)
        return;

    if (PyType_Ready(&PyNetworkInfoType) < 0)
        return;

    if (PyType_Ready(&PyNetworkStateType) < 0)
        return;

    if (PyType_Ready(&PyNetworkType) < 0)
        return;

    if (PyType_Ready(&PyChannelStateType) < 0)
        return;

    if (PyType_Ready(&PyNetworkNickType) < 0)
        return;

    if (PyType_Ready(&PyChannelModeDictType) < 0)
        return;

    if (PyType_Ready(&PyNetworkChannelDictType) < 0)
        return;

    if (PyType_Ready(&PyGListIterType) < 0)
        return;

    if (PyType_Ready(&PyQueryStackType) < 0)
        return;

    if (PyType_Ready(&PyTransportType) < 0)
        return;

    if (PyType_Ready(&PyLinestackType) < 0)
        return;

    if (PyType_Ready(&PyLinestackIterType) < 0)
        return;

    if (PyType_Ready(&PyChannelModeDictType) < 0)
        return;

    if (PyType_Ready(&PyChannelNickDictType) < 0)
        return;

    m = Py_InitModule3("irc", irc_methods, 
                       "Simple IRC protocol module for Python.");
    if (m == NULL)
        return;

    Py_INCREF(&PyLineType);
    PyModule_AddObject(m, "Line", (PyObject *)&PyLineType);
    Py_INCREF(&PyNetworkInfoType);
    PyModule_AddObject(m, "NetworkInfo", (PyObject *)&PyNetworkInfoType);
    Py_INCREF(&PyNetworkStateType);
    PyModule_AddObject(m, "NetworkState", (PyObject *)&PyNetworkStateType);
    Py_INCREF(&PyChannelStateType);
    PyModule_AddObject(m, "ChannelState", (PyObject *)&PyChannelStateType);
    Py_INCREF(&PyNetworkNickType);
    PyModule_AddObject(m, "Nick", (PyObject *)&PyNetworkNickType);
    Py_INCREF(&PyClientType);
    PyModule_AddObject(m, "Client", (PyObject *)&PyClientType);
    Py_INCREF(&PyNetworkType);
    PyModule_AddObject(m, "Network", (PyObject *)&PyNetworkType);
    Py_INCREF(&PyQueryStackType);
    PyModule_AddObject(m, "QueryStack", (PyObject *)&PyQueryStackType);
    Py_INCREF(&PyTransportType);
    PyModule_AddObject(m, "Transport", (PyObject *)&PyTransportType);
    Py_INCREF(&PyLinestackType);
    PyModule_AddObject(m, "Linestack", (PyObject *)&PyLinestackType);
    Py_INCREF(&PyLinestackIterType);
    PyModule_AddIntConstant(m, "TO_SERVER", TO_SERVER);
    PyModule_AddIntConstant(m, "FROM_SERVER", FROM_SERVER);
}

