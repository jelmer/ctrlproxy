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
#include "ctrlproxy.h"

#if 0
cdef extern from "glib.h":
    ctypedef struct GList:
        GList *next
        GList *prev
        void *data
    ctypedef int gboolean
    ctypedef struct GIOChannel
    void *g_malloc0(int)
    void g_free(void *)
    char *g_strdup(char *)

cdef extern from "Python.h":
    void Py_INCREF(object)
    void Py_DECREF(object)

cdef extern from "line.h":
    struct irc_line:
        int argc
        char **args
    irc_line *linedup(irc_line *)
    irc_line *irc_parse_line(char *data)
    void free_line(irc_line *)
    char *irc_line_string(irc_line *)
    char *line_get_nick(irc_line *)


cdef class Line:
    """A RFC2459-compatible line."""
    cdef irc_line *line
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

    def __dealloc__(self):
        free_line(self.line)

    def __str__(self):
        return irc_line_string(self.line)

    def __repr__(self):
        return "Line(%r)" % str(self)

    def get_nick(self):
        """Obtain the nick of the user that sent this line.
        """
        return line_get_nick(self.line)

    def __len__(self):
        return self.line.argc

    def __getitem__(self, i):
        if i >= len(self):
            raise KeyError
        return self.line.args[i]


cdef extern from "state.h":
    ctypedef gboolean irc_modes_t[255]
    struct irc_network_info
    struct irc_network_state:
        irc_network_info *info
        GList *channels
    struct irc_channel_state:
        char *name
        char *key
        char *topic
        int limit
        irc_modes_t modes
    irc_network_state *network_state_init(char *nick, char *username, char *hostname)
    void free_network_state(irc_network_state *state)
    irc_network_info *network_info_init(void *log_fn)
    void free_network_info(irc_network_info *)
    char get_prefix_by_mode(char mode, irc_network_info *)
    int irccmp(irc_network_info *n, char *n1, char *n2)
    int is_prefix(char prefix, irc_network_info *)
    int is_channelname(char *name, irc_network_info *)
    int state_handle_data(irc_network_state *s, irc_line *l)
    irc_channel_state *irc_channel_state_new(char *name)
    void free_channel_state(irc_channel_state *)
    char *mode2string(irc_modes_t modes)
    void string2mode(char *modes, irc_modes_t ar)

cdef class NetworkInfo:
    """Static network information."""
    cdef irc_network_info *info
    cdef object parent
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

    def get_prefix_by_mode(self, mode):
        return chr(get_prefix_by_mode(ord(mode), self.info))

    def irccmp(self, nick1, nick2):
        return irccmp(self.info, nick1, nick2)

    def is_prefix(self, prefix):
        return is_prefix(ord(prefix), self.info)

    def is_channelname(self, name):
        return is_channelname(name, self.info)


cdef class ChannelState:
    cdef irc_channel_state *state
    cdef object parent
    def __init__(self, name):
        self.state = irc_channel_state_new(name)
        self.parent = None

    property name:
        """Name of the channel."""
        def __get__(self): return self.state.name

    property key:
        """Authentication key required to enter the channel."""
        def __get__(self):
            if self.state.key == NULL: return None
            else: return self.state.key
        def __set__(self, value):
            g_free(self.state.key)
            self.state.key = g_strdup(value)

    property limit:
        """Maximum number of users."""
        def __get__(self):
            return self.state.limit
        def __set__(self, int value):
            self.state.limit = value

    property modes:
        def __get__(self):
            cdef char *ret
            ret = mode2string(self.state.modes)
            if ret == NULL:
                return ""
            py_ret = str(ret)
            g_free(ret)
            return py_ret

    property topic:
        def __get__(self):
            if self.state.topic == NULL: return None
            else: return self.state.topic

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
    cdef irc_network_state *state
    cdef object parent
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

    def handle_line(self, Line line):
        """Process a line."""
        return state_handle_data(self.state, line.line)

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

    property info:
        """Network information."""
        def __get__(self):
            cdef NetworkInfo ret
            ret = NetworkInfo()
            ret.set_network_info(self.state.info, self)
            return ret

    property channels:
        """List of known channels."""
        def __get__(self):
            return list(self.iter_channels())


cdef extern from "listener.h":
    struct irc_listener


class Listener:
    pass


cdef extern from "client.h":
    struct irc_transport
    struct irc_client:
        char *default_origin
        void *private_data
        irc_network_state *state
    struct irc_client_callbacks:
        int (*process_from_client) (irc_client *, irc_line *)
        int (*process_to_client) (irc_client *, irc_line *)
        void (*log_fn) (int level, irc_client *, char *text)
        int (*welcome) (irc_client *)
    int client_send_line(irc_client *c, irc_line *)
    void client_invalidate_state(irc_client *c)
    char *client_get_default_target(irc_client *c)
    char *client_get_own_hostmask(irc_client *c)
    int client_set_charset(irc_client *c, char *name)
    irc_client *irc_client_new(irc_transport *t, 
            char *default_origin, char *desc, irc_client_callbacks *,
            void *private_data)

cdef int py_process_from_client(irc_client *client, irc_line *line):
    cdef Line l
    self = <object>client.private_data
    l = Line()
    l.set_line(line)
    self.process_from_client(l)

cdef int py_process_to_client(irc_client *client, irc_line *line):
    cdef Line l
    self = <object>client.private_data
    l = Line()
    l.set_line(line)
    self.process_to_client(l)    

cdef int py_welcome_client(irc_client *client):
    cdef Line l
    self = <object>client.private_data
    ret = self.welcome()
    if ret is not None:
        return True
    return ret


cdef void py_log_client(int level, irc_client *client, char *text):
    self = <object>client.private_data
    self.log(level, text)


cdef irc_transport *new_transport(object o):
    return NULL #FIXME

cdef irc_client_callbacks py_client_callbacks
py_client_callbacks.log_fn = py_log_client
py_client_callbacks.process_from_client = py_process_from_client
py_client_callbacks.process_to_client = py_process_to_client
py_client_callbacks.welcome = py_welcome_client

cdef class Client:
    """An IRC client."""
    cdef irc_client *client

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

    def send_line(self, line):
        """Send a line to this client."""
        if not client_send_line(self.client, <irc_line *>line):
            raise Exception("Error while sending line")

    def invalidate_state(self):
        """Invalidate the state known to the client."""
        client_invalidate_state(self.client)
    
    def get_default_origin(self):
        """Returns the default origin that is used to send lines to this client.
        """
        return self.client.default_origin

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

    def get_default_target(self):
        """Returns the default target name used for this client."""
        return client_get_default_target(self.client)

    def get_own_hostmask(self):
        """Returns the hostmask of the client."""
        return client_get_own_hostmask(self.client)

    def set_charset(self, name):
        """Change the character set.

        :note: None will disable character conversion.
        """
        if not client_set_charset(self.client, name):
            raise Exception("Unable to set character set")

cdef extern from "network.h":
    struct irc_network
    int connect_network(irc_network *)
    int disconnect_network(irc_network *s)
    int network_send_line(irc_network *s, irc_client *c, irc_line *)
    int irc_network_set_charset(irc_network *n, char *name)
    void irc_network_select_next_server(irc_network *n)
    char *network_generate_feature_string(irc_network *n)
    void irc_network_unref(irc_network *)

cdef class Network:
    cdef irc_network *network

    def connect(self):
        if not connect_network(self.network):
            raise Exception("Unable to connect to network")

    def disconnect(self):
        disconnect_network(self.network)
    
    def send_line(self, Line line, Client client=None):
        if not network_send_line(self.network, client.client, line.line):
            raise Exception("Error sending line to network")

    def set_charset(self, charset):
        """Change the character set used to communicate with the network."""
        if not irc_network_set_charset(self.network, charset):
            raise Exception("Unable to set character set")

    def next_server(self):
        """Switch to the next server in the list."""
        irc_network_select_next_server(self.network)

    def feature_string(self):
        """Obtain the feature list for this network."""
        return network_generate_feature_string(self.network)

    def __dealloc__(self):
        irc_network_unref(self.network)
#endif

static PyMethodDef irc_methods[] = { 
    { NULL }
};

void initirc(void)
{
    PyObject *m;

    m = Py_InitModule3("irc", irc_methods, 
                       "Simple IRC protocol module for Python.");
    if (m == NULL)
        return;
}

