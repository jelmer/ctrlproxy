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

#include <Python.h>
#include <stdbool.h>
#include "ctrlproxy.h"
#include "redirect.h"


const char *get_my_hostname() { return NULL; /* FIXME */ }
void log_global(enum log_level ll, const char *fmt, ...) { /* FIXME */}

static PyObject *py_g_list_iter(GList *list, PyObject *parent, PyObject *(*converter) (PyObject *parent, void *));

typedef struct {
    PyObject_HEAD
    GList *iter;
    PyObject *(*converter) (PyObject *parent, void *);
    PyObject *parent;
} PyGListIterObject;

typedef struct {
    PyObject_HEAD
    const struct irc_line *line;
} PyLineObject;


typedef struct {
    PyObject_HEAD
    struct irc_network_state *state;
    PyObject *parent;
} PyNetworkStateObject;

static struct irc_line *PyObject_AsLine(PyObject *obj);

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
    .tp_getset = py_line_getsetters,
    .tp_as_sequence = &py_line_sequence,
};

static struct irc_line *PyObject_AsLine(PyObject *obj)
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

static PyObject *py_network_info_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    PyNetworkInfoObject *self;
    char *kwnames[] = { NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "", kwnames))
        return NULL;

    self = (PyNetworkInfoObject *)type->tp_alloc(type, 0);
    if (self == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    self->parent = NULL;
    self->info = network_info_init();
    if (self->info == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    return (PyObject *)self;
}

static int py_network_info_dealloc(PyNetworkInfoObject *self)
{
    Py_XDECREF(self->parent);
    PyObject_Del(self);
    return 0;
}

PyTypeObject PyNetworkInfoType = {
    .tp_name = "NetworkInfo",
    .tp_new = py_network_info_new,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_dealloc = (destructor)py_network_info_dealloc,
    .tp_methods = py_networkinfo_methods,
    .tp_basicsize = sizeof(PyNetworkInfoObject)
};

typedef struct {
    PyObject_HEAD
    struct irc_channel_state *state;
    PyObject *parent;
} PyChannelStateObject;

static PyObject *py_channel_state_new(PyTypeObject *type, PyObject *args, PyObject *kwargs);
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

typedef struct {
    PyObject_HEAD
    PyChannelStateObject *parent;
} PyChannelModeDictObject;

static int py_channel_mode_dict_dealloc(PyChannelModeDictObject *self)
{
    Py_XDECREF(self->parent);
    PyObject_Del(self);
    return 0;
}

static PyObject *py_channel_mode_dict_get(PyChannelModeDictObject *self, PyObject *py_name)
{
    char mode;

    if (!PyString_Check(py_name) || PyString_Size(py_name) != 1) {
        PyErr_SetNone(PyExc_KeyError);
        return NULL;
    }

    mode = PyString_AsString(py_name)[0];
    if (mode > MAXMODES || mode < 0) {
        PyErr_SetNone(PyExc_KeyError);
        return NULL;
    }
    
    if (self->parent->state->chanmode_option[(uint8_t)mode] == NULL) {
        PyErr_SetNone(PyExc_KeyError);
        return NULL;
    }

    return PyString_FromString(self->parent->state->chanmode_option[(uint8_t)mode]);
}

static int py_channel_mode_dict_set(PyChannelModeDictObject *self, PyObject *py_name, PyObject *py_value)
{
    char mode;

    if (!PyString_Check(py_name) || PyString_Size(py_name) != 1) {
        PyErr_SetNone(PyExc_KeyError);
        return -1;
    }

    mode = PyString_AsString(py_name)[0];
    if (mode > MAXMODES || mode < 0) {
        PyErr_SetNone(PyExc_KeyError);
        return -1;
    }

    if (!PyString_Check(py_value)) {
        PyErr_SetNone(PyExc_TypeError);
        return -1;
    }
    
    if (self->parent->state->chanmode_option[(uint8_t)mode] != NULL) {
        g_free(self->parent->state->chanmode_option[(uint8_t)mode]);
    }

    self->parent->state->chanmode_option[(uint8_t)mode] = g_strdup(PyString_AsString(py_value));

    return 0;
}

static PyMappingMethods py_channel_mode_dict_mapping = {
    .mp_subscript = (binaryfunc)py_channel_mode_dict_get,
    .mp_ass_subscript = (objobjargproc)py_channel_mode_dict_set,
};

PyTypeObject PyChannelModeDictType = {
    .tp_name = "ChannelModeDict",
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_dealloc = (destructor)py_channel_mode_dict_dealloc,
    .tp_basicsize = sizeof(PyChannelModeDictObject),
    .tp_as_mapping = &py_channel_mode_dict_mapping,
};

static PyObject *py_channel_state_get_mode_option(PyChannelStateObject *self, void *closure)
{
    PyChannelModeDictObject *ret = PyObject_New(PyChannelModeDictObject, &PyChannelModeDictType);

    if (ret == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    Py_INCREF(self);
    ret->parent = self;

    return (PyObject *)ret;
}

static PyGetSetDef py_channel_state_getset[] = {
    { "name", (getter)py_channel_state_get_name, NULL, 
        "Name of the channel." },
    { "topic", (getter)py_channel_state_get_topic, NULL,
        "Topic of the channel." },
    { "modes", (getter)py_channel_state_get_modes, NULL,
        "Modes" },
    { "mode_option", (getter)py_channel_state_get_mode_option, NULL,
        "Mode options" },
    { NULL }
};

static int py_channel_state_dealloc(PyChannelStateObject *self)
{
    if (self->parent != NULL) {
        Py_DECREF(self->parent);
    } else {
        free_channel_state(self->state);
    }

    PyObject_Del(self);
    return 0;
}

PyTypeObject PyChannelStateType = {
    .tp_name = "ChannelState",
    .tp_new = py_channel_state_new,
    .tp_dealloc = (destructor)py_channel_state_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_getset = py_channel_state_getset,
    .tp_basicsize = sizeof(PyChannelStateObject)
};

static PyObject *py_channel_state_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    char *name;
    char *kwnames[] = { "name", NULL };
    PyChannelStateObject *self;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s", kwnames, &name))
        return NULL;

    self = PyObject_New(PyChannelStateObject, &PyChannelStateType);
    if (self == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    self->parent = NULL;
    self->state = irc_channel_state_new(name);
    if (self->state == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    return (PyObject *)self;
}



static PyChannelStateObject *py_channel_state_from_ptr(PyObject *parent, struct irc_channel_state *channel)
{
    PyChannelStateObject *ret;
    ret = PyObject_New(PyChannelStateObject, &PyChannelStateType);
    if (ret == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    Py_INCREF(parent);
    ret->parent = parent;
    ret->state = channel;
    return ret;
}

typedef struct {
    PyObject_HEAD
    PyObject *parent;
    struct network_nick *nick;
} PyNetworkNickObject;

static int py_network_nick_dealloc(PyNetworkNickObject *self)
{
    if (self->parent == NULL)
        free_network_nick(NULL, self->nick);
    else
        Py_DECREF(self->parent);
    PyObject_Del(self);
    return 0;
}

static PyObject *py_network_nick_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    char *nick, *username, *host, *hostmask;
    char *kwnames3[] = { "nick", "username", "host", NULL };
    char *kwnames1[] = { "hostmask", NULL };
    PyNetworkNickObject *ret;

    ret = (PyNetworkNickObject *)type->tp_alloc(type, 0);
    if (ret == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    ret->parent = NULL;
    ret->nick = g_new0(struct network_nick, 1);

    if (PyArg_ParseTupleAndKeywords(args, kwargs, "sss", kwnames3, &nick, &username, &host)) {
       network_nick_set_data(ret->nick, nick, username, host);
    } else if (PyArg_ParseTupleAndKeywords(args, kwargs, "s", kwnames1, &hostmask)) {
       network_nick_set_hostmask(ret->nick, hostmask);
    } else {
        g_free(ret->nick);
        type->tp_free(ret);
    }

   return (PyObject *)ret;
}

static PyObject *py_network_nick_get_hostmask(PyNetworkNickObject *self, void *closure)
{
    if (self->nick->hostmask == NULL)
        Py_RETURN_NONE;
    return PyString_FromString(self->nick->hostmask);
}

static int py_network_nick_set_hostmask(PyNetworkNickObject *self, PyObject *value, void *closure)
{
    if (!PyString_Check(value)) {
        PyErr_SetNone(PyExc_TypeError);
        return -1;
    }

    if (!network_nick_set_hostmask(self->nick, PyString_AsString(value))) {
        PyErr_SetNone(PyExc_ValueError);
        return -1;
    }

    return 0;
}



static PyObject *py_network_nick_get_nick(PyNetworkNickObject *self, void *closure)
{
    if (self->nick->nick == NULL)
        Py_RETURN_NONE;
    return PyString_FromString(self->nick->nick);
}

static int py_network_nick_set_nick(PyNetworkNickObject *self, PyObject *value, void *closure)
{
    if (!PyString_Check(value)) {
        PyErr_SetNone(PyExc_TypeError);
        return -1;
    }

    if (!network_nick_set_nick(self->nick, PyString_AsString(value))) {
        PyErr_SetNone(PyExc_ValueError);
        return -1;
    }

    return 0;
}

static PyObject *py_network_nick_get_username(PyNetworkNickObject *self, void *closure)
{
    if (self->nick->username == NULL)
        Py_RETURN_NONE;
    return PyString_FromString(self->nick->username);
}

static int py_network_nick_set_username(PyNetworkNickObject *self, PyObject *value, void *closure)
{
    if (!PyString_Check(value)) {
        PyErr_SetNone(PyExc_TypeError);
        return -1;
    }

    if (!network_nick_set_username(self->nick, PyString_AsString(value))) {
        PyErr_SetNone(PyExc_ValueError);
        return -1;
    }

    return 0;
}

static PyObject *py_network_nick_get_hostname(PyNetworkNickObject *self, void *closure)
{
    if (self->nick->hostname == NULL)
        Py_RETURN_NONE;
    return PyString_FromString(self->nick->hostname);
}

static int py_network_nick_set_hostname(PyNetworkNickObject *self, PyObject *value, void *closure)
{
    if (!PyString_Check(value)) {
        PyErr_SetNone(PyExc_TypeError);
        return -1;
    }

    if (!network_nick_set_hostname(self->nick, PyString_AsString(value))) {
        PyErr_SetNone(PyExc_ValueError);
        return -1;
    }

    return 0;
}

static PyObject *py_network_nick_channel(PyNetworkNickObject *self, struct channel_nick *cn)
{
    return PyString_FromString(cn->channel->name);
}

static PyObject *py_network_nick_channels(PyNetworkNickObject *self, void *closure)
{
    return py_g_list_iter(self->nick->channel_nicks, (PyObject *)self, 
        (PyObject *(*)(PyObject *, void *))py_network_nick_channel);
}

static PyGetSetDef py_network_nick_getsetters[] = {
    { "hostmask", (getter)py_network_nick_get_hostmask, (setter)py_network_nick_set_hostmask, "Hostmask" },
    { "nick", (getter)py_network_nick_get_nick, (setter)py_network_nick_set_nick, "Nick" },
    { "username", (getter)py_network_nick_get_username, (setter)py_network_nick_set_username, "Username" },
    { "hostname", (getter)py_network_nick_get_hostname, (setter)py_network_nick_set_hostname, "Hostname" },
    { "channels", (getter)py_network_nick_channels, NULL, "Channels" },
    { NULL }
};

PyTypeObject PyNetworkNickType = {
    .tp_name = "Nick",
    .tp_new = py_network_nick_new,
    .tp_dealloc = (destructor)py_network_nick_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_getset = py_network_nick_getsetters,
    .tp_basicsize = sizeof(PyNetworkNickObject)
};

static PyNetworkNickObject *py_network_nick_from_ptr(PyObject *parent, struct network_nick *nn)
{
    PyNetworkNickObject *ret;
    ret = PyObject_New(PyNetworkNickObject, &PyNetworkNickType);
    if (ret == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    Py_INCREF(parent);
    ret->parent = parent;
    ret->nick = nn;
    return ret;
}

static PyObject *py_network_state_getitem(PyNetworkStateObject *self, PyObject *py_name)
{
    char *name;
    struct irc_channel_state *channel;
    struct network_nick *nick;

    if (!PyString_Check(py_name)) {
        PyErr_SetNone(PyExc_KeyError);
        return NULL;
    }

    name = PyString_AsString(py_name);

    channel = find_channel(self->state, name);
    if (channel != NULL) {
        return (PyObject *)py_channel_state_from_ptr((PyObject *)self, channel);
    }

    nick = find_network_nick(self->state, name);
    if (nick != NULL) {
        return (PyObject *)py_network_nick_from_ptr((PyObject *)self, nick);
    }

    PyErr_SetNone(PyExc_KeyError);
    return NULL;
}

static Py_ssize_t py_network_state_len(PyNetworkStateObject *self)
{
    return g_list_length(self->state->channels) + g_list_length(self->state->nicks);
}

static PyMappingMethods py_network_state_mapping = {
    .mp_subscript = (binaryfunc)py_network_state_getitem,
    .mp_length = (lenfunc)py_network_state_len,
};


static PyObject *py_network_state_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    char *username = NULL, *nick = NULL, *hostname = NULL;
    char *kwnames[] = { "nick", "username", "hostname", NULL };
    PyNetworkStateObject *self;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|zzz", kwnames, &nick, &username, &hostname))
        return NULL;

    self = (PyNetworkStateObject*)type->tp_alloc(type, 0);
    if (self == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    self->parent = NULL;
    self->state = network_state_init(nick, username, hostname);
    if (self->state == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    return (PyObject *)self;
}

static PyObject *py_network_state_handle_line(PyNetworkStateObject *self, PyObject *py_line)
{
    struct irc_line *l;
    PyObject *ret;

    l = PyObject_AsLine(py_line);
    if (l == NULL)
        return NULL;

    ret = PyInt_FromLong(state_handle_data(self->state, l));

    free_line(l);

    return ret;
}

static PyMethodDef py_network_state_methods[] = {
    { "handle_line", (PyCFunction)py_network_state_handle_line, METH_O,
        "Process a line." },
    { NULL }
};

static PyObject *py_network_state_info(PyNetworkStateObject *self, void *closure)
{
    PyNetworkInfoObject *pyinfo = PyObject_New(PyNetworkInfoObject, &PyNetworkInfoType);

    if (pyinfo == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    Py_INCREF(self);
    pyinfo->parent = (PyObject *)self;
    pyinfo->info = self->state->info;

    return (PyObject *)pyinfo;
}

typedef struct {
    PyObject_HEAD
    PyNetworkStateObject *parent;
} PyNetworkChannelDictObject;

static PyObject *py_network_channel_dict_get(PyNetworkChannelDictObject *self, PyObject *py_name)
{
    char *name;
    struct irc_channel_state *channel;

    if (!PyString_Check(py_name)) {
        PyErr_SetNone(PyExc_KeyError);
        return NULL;
    }

    name = PyString_AsString(py_name);

    channel = find_channel(self->parent->state, name);
    if (channel == NULL) {
        return NULL;
    }

    return (PyObject *)py_channel_state_from_ptr((PyObject *)self->parent, channel);
}

static int py_network_channel_dict_dealloc(PyNetworkChannelDictObject *self)
{
    Py_DECREF(self->parent);
    PyObject_Del(self);
    return 0;
}

static Py_ssize_t py_network_channel_dict_len(PyNetworkChannelDictObject *self)
{
    return g_list_length(self->parent->state->channels);
}

static PyMappingMethods py_network_channel_dict_mapping = {
    .mp_subscript = (binaryfunc)py_network_channel_dict_get,
    .mp_length = (lenfunc)py_network_channel_dict_len,
};

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

static PyObject *py_g_list_iter(GList *list, PyObject *parent, PyObject *(*converter) (PyObject *parent, void *))
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

static PyObject *py_network_channel_dict_iter(PyNetworkChannelDictObject *self)
{
    return py_g_list_iter(self->parent->state->channels, (PyObject *)self, 
                          (PyObject *(*)(PyObject *, void *))py_channel_state_from_ptr);
}

PyTypeObject PyNetworkChannelDictType = {
    .tp_name = "NetworkChannelDict",
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_dealloc = (destructor)py_network_channel_dict_dealloc,
    .tp_basicsize = sizeof(PyNetworkChannelDictObject),
    .tp_as_mapping = &py_network_channel_dict_mapping,
    .tp_iter = (getiterfunc)py_network_channel_dict_iter,
};

static PyObject *py_network_state_channels(PyNetworkStateObject *self, void *closure)
{
    PyNetworkChannelDictObject *ret = PyObject_New(PyNetworkChannelDictObject, &PyNetworkChannelDictType);
    
    if (ret == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    Py_INCREF(self);
    ret->parent = self;
    return (PyObject *)ret;
}

static PyGetSetDef py_network_state_getset[] = {
    { "info", (getter)py_network_state_info, NULL, "Network information" },
    { "channels", (getter)py_network_state_channels, NULL, "Channels" },
    { NULL }
};

static int py_network_state_dealloc(PyNetworkStateObject *self)
{
    if (self->parent != NULL) {
        Py_DECREF(self->parent);
    } else {
        free_network_state(self->state);
    }
    PyObject_Del(self);
    return 0;
}

PyTypeObject PyNetworkStateType = {
    .tp_name = "NetworkState",
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = py_network_state_new,
    .tp_methods = py_network_state_methods,
    .tp_getset = py_network_state_getset,
    .tp_basicsize = sizeof(PyNetworkStateObject),
    .tp_dealloc = (destructor)py_network_state_dealloc,
    .tp_as_mapping = &py_network_state_mapping,
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
    struct irc_line *l;
    if (!PyArg_ParseTuple(args, "O", &py_line))
        return NULL;

    l = PyObject_AsLine(py_line);
    if (l == NULL)
        return NULL;

    if (!client_send_line(self->client, l)) {
        PyErr_SetString(PyExc_RuntimeError, "Error while sending line");
        free_line(l);
        return NULL;
    }
    free_line(l);

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

static PyObject *py_client_get_state(PyClientObject *self, void *closure)
{
    PyNetworkStateObject *ret = PyObject_New(PyNetworkStateObject, &PyNetworkStateType);
    if (ret == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    Py_INCREF(self);
    ret->parent = (PyObject *)self;
    ret->state = self->client->state;

    return (PyObject *)ret;
}

static PyGetSetDef py_client_getsetters[] = {
    { "state", (getter)py_client_get_state, NULL, "State" },
    { NULL }
};

PyTypeObject PyClientType = {
    .tp_name = "Client",
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_methods = py_client_methods,
    .tp_getset = py_client_getsetters,
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
    Py_XDECREF(ret);
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

typedef struct {
    PyObject_HEAD
    struct query_stack *stack;
} PyQueryStackObject;

static PyObject *py_query_stack_record(PyQueryStackObject *self, PyObject *args)
{
    PyObject *py_token, *py_line;
    struct irc_line *line;

    if (!PyArg_ParseTuple(args, "OO", &py_token, &py_line))
        return NULL;

    line = PyObject_AsLine(py_line);
    if (line == NULL)
        return NULL;

    return PyBool_FromLong(query_stack_record(self->stack, py_token, line));
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
    query_stack_free(self->stack);
    PyObject_Del(self);
    return 0;
}

static PyObject *py_query_stack_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    PyQueryStackObject *self = (PyQueryStackObject *)type->tp_alloc(type, 0);
    if (self == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    self->stack = new_query_stack((void (*)(void *))Py_IncRef, (void (*)(void *))Py_DecRef);

    return (PyObject *)self;
}

static PyObject *py_query_stack_entry_from_ptr(PyQueryStackObject *parent, struct query_stack_entry *e)
{
    return Py_BuildValue("(Osl)", e->userdata, e->query->name, e->time);
}

static PyObject *py_query_stack_iter(PyQueryStackObject *self)
{
    return py_g_list_iter(self->stack->entries, (PyObject *)self, (PyObject *(*)(PyObject *, void*))py_query_stack_entry_from_ptr);
}

PyTypeObject PyQueryStackType = {
    .tp_name = "QueryStack",
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = py_query_stack_new,
    .tp_methods = py_query_stack_methods,
    .tp_iter = (getiterfunc)py_query_stack_iter,
    .tp_basicsize = sizeof(PyQueryStackObject),
    .tp_dealloc = (destructor)py_query_stack_dealloc,
};

typedef struct {
    PyObject_HEAD
    struct irc_transport *transport;
} PyTransportObject;

static int py_transport_dealloc(PyTransportObject *self)
{
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

    boolret = (obj == Py_True);

    Py_DECREF(ret);
    return boolret;
}

static void py_transport_disconnect(void *data)
{
    PyObject *obj = (PyObject *)data, *ret;
    /* Nothing */
    ret = PyObject_CallMethod(obj, "disconnect", "");
    Py_XDECREF(ret);
}

static gboolean py_transport_send_line(struct irc_transport *transport, const struct irc_line *l)
{
    PyObject *ret;
    PyLineObject *py_line;

    py_line = (PyLineObject *)PyObject_New(PyLineObject, &PyLineType);
    py_line->line = linedup(l);

    ret = PyObject_CallMethod(transport->backend_data, "send_line", "O", py_line);
    Py_DECREF(py_line);

    if (ret == NULL) {
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

static struct irc_transport_ops py_transport_ops = {
    .free_data = (void (*) (void *))Py_DecRef,
    .is_connected = py_transport_is_connected,
    .disconnect = py_transport_disconnect,
    .send_line = py_transport_send_line,
    .get_peer_name = py_transport_get_peer_name,
    .activate = py_transport_activate,
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

    self->transport = g_new0(struct irc_transport, 1);
	self->transport->pending_lines = g_queue_new();
    self->transport->backend_data = self;
    self->transport->backend_ops = &py_transport_ops;
    return (PyObject *)self;
}

static PyMethodDef py_transport_methods[] = {
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
}

