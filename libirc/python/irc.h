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

#ifndef __LIBIRC_PYTHON_H__
#define __LIBIRC_PYTHON_H__

#include <Python.h>

/* network nick */
PyTypeObject PyNetworkNickType;
typedef struct {
    PyObject_HEAD
    PyObject *parent;
    struct network_nick *nick;
} PyNetworkNickObject;


/* network info */
PyTypeObject PyNetworkInfoType;
typedef struct {
    PyObject_HEAD
    struct irc_network_info *info;
    PyObject *parent;
} PyNetworkInfoObject;


PyTypeObject PyNetworkChannelDictType;
PyTypeObject PyChannelNickDictType;
PyTypeObject PyChannelModeDictType;

/* transport */

typedef struct {
    PyObject_HEAD
    struct irc_transport *transport;
    PyObject *parent;
} PyTransportObject;
PyTypeObject PyTransportType;
struct irc_transport *wrap_py_transport(PyObject *obj);
struct irc_transport_ops py_transport_ops;

/* linestack */

typedef struct {
    PyObject_HEAD
    struct linestack_context *linestack;
    PyObject *parent;
} PyLinestackObject;
PyTypeObject PyLinestackType;

/* GList iterator */
PyObject *py_g_list_iter(GList *list, PyObject *parent, PyObject *(*converter) (PyObject *parent, void *));

typedef struct {
    PyObject_HEAD
    GList *iter;
    PyObject *(*converter) (PyObject *parent, void *);
    PyObject *parent;
} PyGListIterObject;

/* line */
PyTypeObject PyLineType;
typedef struct {
    PyObject_HEAD
    const struct irc_line *line;
} PyLineObject;

struct irc_line *PyObject_AsLine(PyObject *obj);

/* network state */
PyTypeObject PyNetworkStateType;
typedef struct {
    PyObject_HEAD
    struct irc_network_state *state;
    PyObject *parent;
} PyNetworkStateObject;

struct irc_network_state *PyObject_AsNetworkState(PyObject *obj);

PyTypeObject PyChannelStateType;

typedef struct {
    PyObject_HEAD
    struct irc_channel_state *state;
    PyObject *parent;
} PyChannelStateObject;

void g_error_set_python(GError **error);

/* network */
PyTypeObject PyNetworkType;
typedef struct {
    PyObject_HEAD
    struct irc_network *network;
} PyNetworkObject;

/* query stack */
typedef struct {
    PyObject_HEAD
    struct query_stack *stack;
    PyObject *parent;
    void *(*export_userdata) (PyObject *);
    PyObject *(*import_userdata) (void *);
} PyQueryStackObject;
PyTypeObject PyQueryStackType;

/* client */
struct irc_client *PyObject_AsClient(PyObject *obj);

#endif
