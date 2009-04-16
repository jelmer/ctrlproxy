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
PyTypeObject PyChannelModeDictType;

/* transport */

typedef struct {
    PyObject_HEAD
    struct irc_transport *transport;
    PyObject *parent;
} PyTransportObject;PyTypeObject PyTransportType;
struct irc_transport *wrap_py_transport(PyObject *obj);

/* GList iterator */
PyObject *py_g_list_iter(GList *list, PyObject *parent, PyObject *(*converter) (PyObject *parent, void *));

typedef struct {
    PyObject_HEAD
    GList *iter;
    PyObject *(*converter) (PyObject *parent, void *);
    PyObject *parent;
} PyGListIterObject;

/* line */

typedef struct {
    PyObject_HEAD
    const struct irc_line *line;
} PyLineObject;

struct irc_line *PyObject_AsLine(PyObject *obj);

/* network state */

typedef struct {
    PyObject_HEAD
    struct irc_network_state *state;
    PyObject *parent;
} PyNetworkStateObject;


#endif
