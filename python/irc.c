/*    ctrlproxy: A modular IRC proxy
 *    (c) 2002-2007 Jelmer Vernooij <jelmer@nl.linux.org>
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

#include <Python.h>
#include <stdbool.h>
#include "ctrlproxy.h"

const char *get_my_hostname() { return NULL; /* FIXME */ }
void log_global(enum log_level ll, const char *fmt, ...) { /* FIXME */}

typedef struct {
    PyObject_HEAD
    const struct irc_line *line;
} PyLineObject;

PyObject *py_line_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    return NULL; /* FIXME */
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
    ret = PyString_FromFormat("Line(%s)", str);
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

static PyTypeObject PyLineType = {
    .tp_name = "Line",
    .tp_new = py_line_new,
    .tp_dealloc = (destructor)py_line_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_basicsize = sizeof(PyLineObject),
    .tp_str = (reprfunc)py_line_str,
    .tp_repr = (reprfunc)py_line_repr,
    .tp_methods = py_line_methods,
    .tp_doc = "A RFC2459-compatible line.",
    .tp_as_sequence = &py_line_sequence,
};

static const struct irc_line *PyObject_AsLine(PyObject *obj)
{
    if (PyString_Check(obj))
        return irc_parse_line(PyString_AsString(obj));

    if (PyObject_TypeCheck(obj, &PyLineType))
        return ((PyLineObject *)obj)->line;

    PyErr_SetString(PyExc_TypeError, "Expected line");
    return NULL;
}

typedef struct {
    PyObject_HEAD
    struct irc_network_info *info;
    PyObject *parent;
} PyNetworkInfoObject;

static PyObject *py_networkinfo_is_prefix(PyNetworkInfoObject *self, PyObject *args)
{
    char *prefix;
    if (!PyArg_ParseTuple(args, "s", &prefix))
        return NULL;

    if (is_prefix(prefix[0], self->info)) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
}

static PyObject *py_networkinfo_get_prefix_by_mode(PyNetworkInfoObject *self, PyObject *args)
{
    char *mode;
    char ret[2] = { 0, 0 };
    if (!PyArg_ParseTuple(args, "s", &mode))
        return NULL;

    ret[0] = get_prefix_by_mode(mode[0], self->info);

    return PyString_FromString(ret);
}

static PyObject *py_networkinfo_irccmp(PyNetworkInfoObject *self, PyObject *args)
{
    char *nick1, *nick2;
    if (!PyArg_ParseTuple(args, "ss", &nick1, &nick2))
        return NULL;

    return PyInt_FromLong((long)irccmp(self->info, nick1, nick2));
}

static PyObject *py_networkinfo_is_channelname(PyNetworkInfoObject *self, PyObject *args)
{
    char *name;
    if (!PyArg_ParseTuple(args, "s", &name))
        return NULL;

    if (is_channelname(name, self->info)) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
}


static PyMethodDef py_networkinfo_methods[] = {
    { "is_prefix", (PyCFunction)py_networkinfo_is_prefix, METH_VARARGS,
        "is_prefix(char) -> bool\n" },
    { "get_prefix_by_mode", (PyCFunction)py_networkinfo_get_prefix_by_mode,
        METH_VARARGS, "get_prefix_by_mode(mode) -> prefix" },
    { "irccmp", (PyCFunction)py_networkinfo_irccmp, METH_VARARGS,
        "irccmp(nick1, nick2) -> int" },
    { "is_channelname", (PyCFunction)py_networkinfo_is_channelname, METH_VARARGS,
        "is_channelname(name) -> bool" },
    { NULL }
};

PyTypeObject PyNetworkInfoType = {
    .tp_name = "NetworkInfo",
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_methods = py_networkinfo_methods,
    .tp_basicsize = sizeof(PyNetworkInfoObject)
};

typedef struct {
    PyObject_HEAD
    struct irc_channel_state *state;
    PyObject *parent;
} PyChannelStateObject;

static PyObject *py_channel_state_get_name(PyChannelStateObject *self, void *closure)
{
    return PyString_FromString(self->state->name);
}

static PyObject *py_channel_state_get_topic(PyChannelStateObject *self, void *closure)
{
    if (self->state->topic == NULL)
        Py_RETURN_NONE;
    return PyString_FromString(self->state->topic);
}

static PyObject *py_channel_state_get_modes(PyChannelStateObject *self, void *closure)
{
    char *ret;
    PyObject *py_ret;
    ret = mode2string(self->state->modes);
    if (ret == NULL)
        return PyString_FromString("");
    py_ret = PyString_FromString(ret);
    g_free(ret);
    return py_ret;
}

static PyGetSetDef py_channel_state_getset[] = {
    { "name", (getter)py_channel_state_get_name, NULL, 
        "Name of the channel." },
    { "topic", (getter)py_channel_state_get_topic, NULL,
        "Topic of the channel." },
    { "modes", (getter)py_channel_state_get_modes, NULL,
        "Modes" },
    { NULL }
};

PyTypeObject PyChannelStateType = {
    .tp_name = "ChannelState",
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_getset = py_channel_state_getset,
    .tp_basicsize = sizeof(PyChannelStateObject)
};

typedef struct {
    PyObject_HEAD
    struct irc_network_state *state;
    PyObject *parent;
} PyNetworkStateObject;

static PyObject *py_network_state_handle_line(PyNetworkStateObject *self, PyObject *args)
{
    PyObject *py_line;
    const struct irc_line *l;

    if (!PyArg_ParseTuple(args, "O", &py_line))
        return NULL;
    
    l = PyObject_AsLine(py_line);
    if (l == NULL)
        return NULL;
    return PyInt_FromLong(state_handle_data(self->state, l));
}

static PyMethodDef py_network_state_methods[] = {
    { "handle_line", (PyCFunction)py_network_state_handle_line, METH_VARARGS,
        "Process a line." },
    { NULL }
};

static PyObject *py_network_state_info(PyNetworkStateObject *self, void *closure)
{
    return NULL; /* FIXME */
}

static PyGetSetDef py_network_state_getset[] = {
    { "info", (getter)py_network_state_info, NULL, "Network information" },
    { NULL }
};

PyTypeObject PyNetworkStateType = {
    .tp_name = "NetworkState",
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_methods = py_network_state_methods,
    .tp_getset = py_network_state_getset,
    .tp_basicsize = sizeof(PyNetworkStateObject)
};

typedef struct {
    PyObject_HEAD
    struct irc_client *client;
} PyClientObject;

static PyObject *py_client_get_default_target(PyClientObject *self)
{
    return PyString_FromString(client_get_default_target(self->client));
}

static PyObject *py_client_get_own_hostmask(PyClientObject *self)
{
    return PyString_FromString(client_get_own_hostmask(self->client));
}

static PyObject *py_client_get_default_origin(PyClientObject *self)
{
    return PyString_FromString(self->client->default_origin);
}

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
    const struct irc_line *l;
    if (!PyArg_ParseTuple(args, "O", &py_line))
        return NULL;

    l = PyObject_AsLine(py_line);
    if (l == NULL)
        return NULL;

    if (!client_send_line(self->client, l)) {
        PyErr_SetString(PyExc_RuntimeError, "Error while sending line");
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyMethodDef py_client_methods[] = {
    { "get_default_target", (PyCFunction)py_client_get_default_target,
        METH_NOARGS, 
        "Returns the default target name used for this client." },
    { "get_own_hostmask", (PyCFunction)py_client_get_own_hostmask,
        METH_NOARGS,
        "Returns the hostmask of the client." },
    { "get_default_origin", (PyCFunction)py_client_get_default_origin,
        METH_NOARGS,
        "Returns the default origin that is used to send lines to this client." },
    { "set_charset", (PyCFunction)py_client_set_charset, 
        METH_VARARGS,
        "Change the character set."  
        ":note: None will disable character conversion."
    },
    { "send_line", (PyCFunction)py_client_send_line,
        METH_VARARGS,
        "Send a line to this client." },
    { NULL }
};

PyTypeObject PyClientType = {
    .tp_name = "Client",
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_methods = py_client_methods,
    .tp_basicsize = sizeof(PyClientObject)
};

typedef struct {
    PyObject_HEAD
    struct irc_network *network;
} PyNetworkObject;

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
    const struct irc_line *l;

    if (!PyArg_ParseTuple(args, "O|O", &py_line, &py_client))
        return NULL;

    l = PyObject_AsLine(py_line);
    if (l == NULL)
        return NULL;
    if (!network_send_line(self->network, NULL, l, TRUE)) {
        PyErr_SetString(PyExc_RuntimeError, "Error sending line to network");
        return NULL;
    }
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
}

PyTypeObject PyNetworkType = {
    .tp_name = "Network",
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_methods = py_network_methods,
    .tp_basicsize = sizeof(PyNetworkObject),
    .tp_dealloc = (destructor)py_network_dealloc,
};

static int py_process_from_client(struct irc_client *client, const struct irc_line *line)
{
    PyObject *self, *ret;
    PyLineObject *py_line;
    py_line = PyObject_New(PyLineObject, &PyLineType);
    py_line->line = line;
    ret = PyObject_CallMethod(self, "process_from_client", "O", py_line);
    Py_DECREF(py_line);
    Py_XDECREF(ret);
}

static void py_log_client(enum log_level level, const struct irc_client *client, const char *text)
{
    PyObject *self, *ret;
    self = client->private_data;
    ret = PyObject_CallMethod(self, "log", "is", level, text);
    Py_XDECREF(ret);
}

static int py_process_to_client(struct irc_client *client, const struct irc_line *line)
{
    PyObject *self, *ret;
    PyLineObject *py_line;

    self = client->private_data;
    py_line = PyObject_New(PyLineObject, &PyLineType);
    py_line->line = line;

    ret = PyObject_CallMethod(self, "process_to_client", "O", py_line);
    Py_DECREF(py_line);
    Py_XDECREF(ret);
}

static int py_welcome_client(struct irc_client *client)
{
    PyObject *ret, *self;
    self = client->private_data;
    ret = PyObject_CallMethod(self, "welcome", "");
    if (ret == NULL)
        return false;
    if (ret == Py_True)
        return true;
    return false;
}

static const struct irc_client_callbacks py_client_callbacks = {
    .log_fn = py_log_client,
    .process_from_client = py_process_from_client,
    .process_to_client = py_process_to_client,
    .welcome = py_welcome_client
};

#if 0
cdef class Line:
    def __init__(self, data):
        if isinstance(data, str):
            self.line = irc_parse_line(data)
        elif isinstance(data, Line):
            self.line = linedup((<Line>data).line)
        elif isinstance(data, list):
            raise NotImplementedError
        else:
            raise ValueError

    cdef void set_line(self, irc_line *line):
        self.line = line



cdef class NetworkInfo:
    """Static network information."""
    def __init__(self):
        self.info = network_info_init(NULL)
        self.parent = None

    cdef void set_network_info(self, irc_network_info *info, parent):
        self._free()
        self.info = info
        self.parent = parent
        Py_INCREF(self.parent)

    def _free(self):
        if self.parent is None:
            free_network_info(self.info)
        else:
            Py_DECREF(self.parent)

    def __dealloc__(self):
        self._free()


cdef class ChannelState:
    def __init__(self, name):
        self.state = irc_channel_state_new(name)
        self.parent = None

    cdef void set_channel_state(self, irc_channel_state *cs, parent):
        self._free()
        self.state = cs
        self.parent = parent
        Py_INCREF(self.parent)

    def _free(self):
        if self.parent is None:
            free_channel_state(self.state)
        else:
            Py_DECREF(self.parent)
 
    def __dealloc__(self):
        self._free()


cdef class GListIter:
    cdef GList *list
    cdef object (*convert_fn) (void *, object)
    def __init__(self):
        self.list = NULL
        self.convert_fn = NULL

    def __iter__(self):
        return self

    def __next__(self):
        if self.list == NULL:
            raise StopIteration
        ret = self.convert_fn(self.list.data, self)
        self.list = self.list.next
        return ret


cdef new_channel_state(void *cs, parent):
    cdef ChannelState ret
    ret = ChannelState("")
    ret.set_channel_state(<irc_channel_state *>cs, parent)
    return ret


cdef class NetworkState:
    def __init__(self, char *nick, char *username, char *hostname):
        self.state = network_state_init(nick, username, hostname)
        self.parent = None

    cdef void set_network_state(self, irc_network_state *state, parent):
        self._free()
        self.state = state
        self.parent = parent
        Py_INCREF(self.parent)

    def __dealloc__(self):
        self._free()

    def _free(self):
        if self.parent is None:
            free_network_state(self.state)
        else:
            Py_DECREF(self.parent)

    def iter_channels(self):
        """Iterate over the known channels."""
        cdef GListIter ret
        ret = GListIter()
        ret.list = self.state.channels
        ret.convert_fn = new_channel_state
        return ret

    def __getitem__(self, name):
        for c in self.iter_channels():
            if c.name == name:
                return c
        raise KeyError

    property channels:
        """List of known channels."""
        def __get__(self):
            return list(self.iter_channels())

class Listener:
    pass

cdef irc_transport *new_transport(object o):
    return NULL #FIXME

cdef class Client:
    """An IRC client."""
    def __init__(self, transport, default_origin, description=None):
        """Create a new IRC client.

        :param transport: file-like Python object used for communication
        :param default_origin: Default origin
        :param description: Optional description for this client
        """
        self.client = irc_client_new(new_transport(transport),
                                     default_origin, description,
                                     &py_client_callbacks,
                                     <void *>self)
        if self.client == NULL:
            raise Exception("Unable to create client")

    property state:
        def __get__(self):
            cdef NetworkState ret
            ret = NetworkState("", "", "")
            ret.set_network_state(self.client.state, self)
            return ret

    def process_from_client(self, line):
        """Called for each line sent by the client."""
        raise NotImplementedError

    def process_to_client(self, line):
        """Called for each line sent to the client."""
        raise NotImplementedError

    def welcome(self):
        """Called when the client is authenticated. """
        pass

    def log(self, level, text):
        """Called for each log event.
        
        :param level: The log level associated with the event.
        :param text: Log message.
        """
        raise NotImplementedError
#endif

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
    Py_INCREF(&PyClientType);
    PyModule_AddObject(m, "Client", (PyObject *)&PyClientType);
    Py_INCREF(&PyNetworkType);
    PyModule_AddObject(m, "Network", (PyObject *)&PyNetworkType);
}

