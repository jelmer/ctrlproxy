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

struct irc_network_state *PyObject_AsNetworkState(PyObject *obj)
{
    if (!PyObject_TypeCheck(obj, &PyNetworkStateType)) {
        PyErr_SetNone(PyExc_TypeError);
        return NULL;
    }

    return ((PyNetworkStateObject *)obj)->state;
}

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

static int py_channel_state_set_topic(PyChannelStateObject *self, PyObject *value, void *closure)
{
    if (value != Py_None && !PyString_Check(value)) {
        PyErr_SetNone(PyExc_TypeError);
        return -1;
    }

    if (self->state->topic == NULL)
        g_free(self->state->topic);

    if (value == Py_None) {
        self->state->topic = NULL;
    } else {
        self->state->topic = g_strdup(PyString_AsString(value));
    }

    return 0;
}

static int py_channel_state_set_topic_time(PyChannelStateObject *self, PyObject *value, void *closure)
{
    if (!PyInt_Check(value) && !PyLong_Check(value)) {
        PyErr_SetNone(PyExc_TypeError);
        return -1;
    }

    if (PyInt_Check(value))
        self->state->topic_set_time = PyInt_AsLong(value);
    if (PyLong_Check(value))
        self->state->topic_set_time = PyLong_AsLong(value);

    return 0;
}

static int py_channel_state_set_time(PyChannelStateObject *self, PyObject *value, void *closure)
{
    if (!PyInt_Check(value) && !PyLong_Check(value)) {
        PyErr_SetNone(PyExc_TypeError);
        return -1;
    }

    if (PyInt_Check(value))
        self->state->creation_time = PyInt_AsLong(value);
    if (PyLong_Check(value))
        self->state->creation_time = PyLong_AsLong(value);

    return 0;
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

static int py_channel_state_set_modes(PyChannelStateObject *self, PyObject *value, void *closure)
{
    char *modestr;

    if (!PyString_Check(value) && value != Py_None) {
        PyErr_SetNone(PyExc_TypeError);
        return -1;
    }

    if (value == Py_None)
        modestr = NULL;
    else
        modestr = PyString_AsString(value);

    if (!string2mode(modestr, self->state->modes)) {
        PyErr_SetNone(PyExc_ValueError);
        return -1;
    }

    return 0;
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

static PyObject *py_channel_state_get_topic_time(PyChannelStateObject *self, void *closure)
{
    return PyLong_FromLong(self->state->topic_set_time);
}

static PyObject *py_channel_state_get_time(PyChannelStateObject *self, void *closure)
{
    return PyLong_FromLong(self->state->creation_time);
}

typedef struct {
    PyObject_HEAD
    PyChannelStateObject *parent;
} PyChannelNickDictObject;

static int py_channel_nick_dict_dealloc(PyChannelNickDictObject *self)
{
    Py_DECREF(self->parent);
    PyObject_Del(self);
    return 0;
}

static Py_ssize_t py_channel_nick_dict_len(PyChannelNickDictObject *self)
{
    return g_list_length(self->parent->state->nicks);
}

static struct channel_nick *py_channel_nick_dict_find_pyobject(PyChannelNickDictObject *self, PyObject *py_name)
{
    struct channel_nick *cn;
    char *name;
    if (!PyString_Check(py_name)) {
        PyErr_SetNone(PyExc_KeyError);
        return NULL;
    }

    name = PyString_AsString(py_name);

    cn = find_channel_nick(self->parent->state, name);

    if (cn == NULL) {
        PyErr_SetNone(PyExc_KeyError);
        return NULL;
    }

    return cn;
}

static PyObject *py_channel_nick_dict_get_nick_mode(PyChannelNickDictObject *self, PyObject *py_name)
{
    struct channel_nick *cn;
    char *modestr;
    PyObject *ret;

    cn = py_channel_nick_dict_find_pyobject(self, py_name);
    if (cn == NULL) {
        /* py_channel_nick_dict_find_pyobject already sets error */
        return NULL;
    }

    modestr = mode2string(cn->modes);

    if (modestr == NULL) {
        Py_RETURN_NONE;
    }

    ret = PyString_FromString(modestr);

    g_free(modestr);

    return ret;
}

static PyObject *py_channel_nick_dict_subscript(PyChannelNickDictObject *self, PyObject *py_name)
{
    PyNetworkNickObject *ret;
    struct channel_nick *cn;

    cn = py_channel_nick_dict_find_pyobject(self, py_name);
    if (cn == NULL) {
        /* py_channel_nick_dict_find_pyobject already sets error */
        return NULL;
    }

    ret = PyObject_New(PyNetworkNickObject, &PyNetworkNickType);
    if (ret == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    Py_INCREF(self->parent);
    ret->parent = (PyObject *)self->parent;
    ret->nick = cn->global_nick;

    return (PyObject *)ret;
}

static PyMappingMethods py_channel_nick_dict_mapping = {
    .mp_length = (lenfunc)py_channel_nick_dict_len,
    .mp_subscript = (binaryfunc)py_channel_nick_dict_subscript,
};

static PyObject *py_channel_nick_dict_add(PyChannelNickDictObject *self, PyObject *args)
{
    struct channel_nick *cn;
    PyNetworkNickObject *py_nick;
    char *modestr = NULL;
    struct irc_channel_state *cs;

    if (!PyArg_ParseTuple(args, "O|s", &py_nick, &modestr))
        return NULL;

    if (!PyObject_TypeCheck(py_nick, &PyNetworkNickType)) {
        PyErr_SetNone(PyExc_TypeError);
        return NULL;
    }

    cs = self->parent->state;

    cn = find_channel_nick(cs, py_nick->nick->nick);
    if (cn != NULL) {
        PyErr_SetNone(PyExc_KeyError);
        return NULL;
    }

    if (py_nick->parent == NULL) {
        Py_INCREF(self->parent);
        py_nick->parent = (PyObject *)self->parent;
        if (cs->network != NULL)
            cs->network->nicks = g_list_append(cs->network->nicks, py_nick->nick);
    } else {
        /* FIXME: What if we're adding the same nick to multiple channels ? */
        PyErr_SetNone(PyExc_TypeError);
        return NULL;
    }

   	cn = g_new0(struct channel_nick,1);
	g_assert(cn != NULL);
	
    string2mode(modestr, cn->modes);
	cn->channel = cs;
	cn->global_nick = py_nick->nick;
	cs->nicks = g_list_append(cs->nicks, cn);
    cn->global_nick->channel_nicks = g_list_append(cn->global_nick->channel_nicks, cn);

    Py_RETURN_NONE;
}

static PyMethodDef py_channel_nick_dict_methods[] = {
    { "nick_mode", (PyCFunction)py_channel_nick_dict_get_nick_mode,
        METH_O, "Get nick mode" },
    { "add", (PyCFunction)py_channel_nick_dict_add,
        METH_VARARGS, "Add a nick" },
    { NULL }
};

PyTypeObject PyChannelNickDictType = {
    .tp_name = "ChannelNickDict",
    .tp_dealloc = (destructor)py_channel_nick_dict_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_basicsize = sizeof(PyChannelNickDictObject),
    .tp_as_mapping = &py_channel_nick_dict_mapping,
    .tp_methods = py_channel_nick_dict_methods,
};

static PyObject *py_channel_state_get_nicks(PyChannelStateObject *self, void *closure)
{
    PyChannelNickDictObject *ret = PyObject_New(PyChannelNickDictObject, &PyChannelNickDictType);
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
    { "topic", (getter)py_channel_state_get_topic,
               (setter)py_channel_state_set_topic,
        "Topic of the channel." },
    { "topic_set_time", (getter)py_channel_state_get_topic_time,
        (setter)py_channel_state_set_topic_time,
        "Time the topic was set." },
    { "creation_time",
        (getter)py_channel_state_get_time,
        (setter)py_channel_state_set_time,
        "Timestamp when the channel was created" },
    { "modes", (getter)py_channel_state_get_modes,
        (setter)py_channel_state_set_modes,
        "Modes" },
    { "mode_option", (getter)py_channel_state_get_mode_option, NULL,
        "Mode options" },
    { "nicks", (getter)py_channel_state_get_nicks, NULL,
        "Nicks on this channel" },
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
    struct network_nick *nn;

    int len = ((args == NULL)?0:PyTuple_Size(args)) +
              ((kwargs == NULL)?0:PyDict_Size(kwargs));

    if (len == 1) {
        if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s", kwnames1, &hostmask))
            return NULL;
        nn = g_new0(struct network_nick, 1);

        if (!network_nick_set_hostmask(nn, hostmask)) {
            g_free(nn);
            PyErr_SetNone(PyExc_ValueError);
            return NULL;
        }
    } else if (len == 3) {
        if (!PyArg_ParseTupleAndKeywords(args, kwargs, "sss", kwnames3, &nick, &username, &host))
            return NULL;
       nn = g_new0(struct network_nick, 1);
       network_nick_set_data(nn, nick, username, host);
    } else {
        PyErr_SetNone(PyExc_ValueError);
        return NULL;
    }

    ret = (PyNetworkNickObject *)type->tp_alloc(type, 0);
    if (ret == NULL) {
        g_free(nn);
        PyErr_NoMemory();
        return NULL;
    }

    ret->parent = NULL;
    ret->nick = nn;

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

static PyObject *py_network_nick_get_modes(PyNetworkNickObject *self, void *closure)
{
    char *modestr;
    PyObject *ret;

    modestr = mode2string(self->nick->modes);
    if (modestr == NULL)
        Py_RETURN_NONE;

    ret = PyString_FromString(modestr);

    g_free(modestr);

    return ret;
}

static int py_network_nick_set_modes(PyNetworkNickObject *self, PyObject *value, void *closure)
{
    if (!PyString_Check(value)) {
        PyErr_SetNone(PyExc_TypeError);
        return -1;
    }
    if (!string2mode(PyString_AsString(value), self->nick->modes)) {
        PyErr_SetNone(PyExc_ValueError);
        return -1;
    }
    return 0;
}

static PyGetSetDef py_network_nick_getsetters[] = {
    { "hostmask", (getter)py_network_nick_get_hostmask, (setter)py_network_nick_set_hostmask, "Hostmask" },
    { "nick", (getter)py_network_nick_get_nick, (setter)py_network_nick_set_nick, "Nick" },
    { "username", (getter)py_network_nick_get_username, (setter)py_network_nick_set_username, "Username" },
    { "hostname", (getter)py_network_nick_get_hostname, (setter)py_network_nick_set_hostname, "Hostname" },
    { "channels", (getter)py_network_nick_channels, NULL, "Channels" },
    { "modes", (getter)py_network_nick_get_modes,
        (setter)py_network_nick_set_modes, "Modes" },
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

static PyObject *py_network_state_add(PyNetworkStateObject *self, PyObject *obj)
{
    if (PyObject_TypeCheck(obj, &PyChannelStateType)) {
        PyChannelStateObject *chobj = (PyChannelStateObject *)obj;
        GList *gl;

        if (find_channel(self->state, chobj->state->name) != NULL) {
            PyErr_SetString(PyExc_KeyError, "Key already exists");
            return NULL;
        }

        if (chobj->parent != NULL) {
            PyErr_SetString(PyExc_KeyError, "Channel is already linked to a state");
            return NULL;
        }

        self->state->channels = g_list_append(self->state->channels, chobj->state);

        Py_INCREF(self);
        chobj->state->network = self->state;
        chobj->parent = (PyObject *)self;

        for (gl = chobj->state->nicks; gl != NULL; gl = gl->next) {
            struct channel_nick *cn = gl->data;

            self->state->nicks = g_list_append(self->state->nicks, cn->global_nick);
        }

        Py_RETURN_NONE;
    } else if (PyObject_TypeCheck(obj, &PyNetworkNickType)) {
        PyNetworkNickObject *nickobj = (PyNetworkNickObject *)obj;

        if (find_network_nick(self->state, nickobj->nick->nick) != NULL) {
            PyErr_SetString(PyExc_KeyError, "Key already exists");
            return NULL;
        }

        if (nickobj->parent != NULL) {
            PyErr_SetString(PyExc_KeyError, "Nick is already linked to a state");
            return NULL;
        }

        self->state->nicks = g_list_append(self->state->nicks, nickobj->nick);

        Py_INCREF(self);
        nickobj->parent = (PyObject *)self;

        Py_RETURN_NONE;
    } else {
        PyErr_SetNone(PyExc_TypeError);
        return NULL;
    }
}

static PyMethodDef py_network_state_methods[] = {
    { "handle_line", (PyCFunction)py_network_state_handle_line, METH_O,
        "Process a line." },
    { "add", (PyCFunction)py_network_state_add, METH_O,
        "Add a channel or nick" },
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
        PyErr_SetNone(PyExc_KeyError);
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

static PyObject *py_network_channel_dict_iter(PyNetworkChannelDictObject *self)
{
    return py_g_list_iter(self->parent->state->channels, (PyObject *)self,
                          (PyObject *(*)(PyObject *, void *))py_channel_state_from_ptr);
}

static PyObject *py_network_channel_dict_keys(PyNetworkChannelDictObject *self)
{
    GList *gl;
    int i = 0;
    PyObject *ret = PyList_New(g_list_length(self->parent->state->channels));

    for (gl = self->parent->state->channels; gl; gl = gl->next) {
        struct irc_channel_state *cs = gl->data;
        PyList_SetItem(ret, i, PyString_FromString(cs->name));
        i++;
    }

    return ret;
}

static PyMethodDef py_network_channel_dict_methods[] = {
    { "keys", (PyCFunction)py_network_channel_dict_keys,
        METH_NOARGS, "Keys" },
    { NULL }
};

PyTypeObject PyNetworkChannelDictType = {
    .tp_name = "NetworkChannelDict",
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_dealloc = (destructor)py_network_channel_dict_dealloc,
    .tp_basicsize = sizeof(PyNetworkChannelDictObject),
    .tp_methods = py_network_channel_dict_methods,
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
