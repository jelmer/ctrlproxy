/*
	ctrlproxy: A modular IRC proxy
	python: scriptable module with python interface for ctrlproxy
	admin commands
	 * PYTHON LOAD <file>
	 * PYTHON UNLOAD <file>
	 * PYTHON LIST

	(c) 2003 Daniel Poelzleithner <ctrlproxy@poelzi.org>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

//#define _GNU_SOURCE
//#include <string.h>
#include "Python.h"
#include "structmember.h"
#include "ctrlproxy.h"
#include <string.h>
#include "irc.h"
#include "admin.h"
#include "libxml_wrap.h"

xmlNodePtr xmlConf;

static GList *py_rcv_hooks = NULL;
static GList *py_lose_client_hooks = NULL;
static GList *py_new_client_hooks = NULL;
static GList *py_motd_hooks = NULL;
static GList *py_server_connected_hooks  = NULL;
static GList *py_server_disconnected_hooks = NULL;
static GList *py_events = NULL;

static GList *loaded_scripts = NULL;
static GList *script_attributes = NULL;

static GList *admin_commands = NULL;

static PyObject *xml_module = NULL;
static PyObject *xml_modulemod = NULL;
static PyObject *xml_node = NULL;

static char *pylibdir = PYLIBDIR;

// Thread support
PyThreadState * mainThreadState = NULL;

static int sid = 0;
static int py_redirect_stdout = 0;
static int py_redirect_stderr = 0;

#define PYCTRLPROXY_NODETOPYOB(node) \
PyInstance_New(xml_node,Py_BuildValue("(O)",libxml_xmlNodePtrWrap((xmlNodePtr) node)),NULL);

#define PYCTRLPROXY_PYADDCALLBACK(function,callarray) \
 static PyObject * function(PyObject *self, PyObject *args, PyObject *keywds) { \
    PyObject *result = NULL; \
    PyObject *temp; \
	struct callback_object *c; \
    if (PyArg_ParseTuple(args, "O:set_callback", &temp)) { \
        if (!PyCallable_Check(temp)) { \
            PyErr_SetString(PyExc_TypeError, "parameter must be callable"); \
            return NULL; \
        } \
\
		c = malloc(sizeof(struct callback_object)); \
		c->thread = PyThreadState_Get(); \
		c->callback = temp;\
		Py_XINCREF(c->callback);         /* Add a reference to new callback */ \
\
        callarray = g_list_append(callarray, c); \
\
        Py_INCREF(Py_None); \
        result = Py_None; \
    } \
    return result; \
}

#define PYCTRLPROXY_PYCALLPYCALLBACKS(LIST,CREATE) \
	GList *g = LIST; \
	PyObject *r = NULL; \
	PyThreadState *tstate; \
	while(g) { \
		struct callback_object *c = (struct callback_object *)g->data; \
		PyObject *lo = CREATE(l); \
		PyObject *arglist = Py_BuildValue("(O)", lo); \
		PyEval_AcquireLock(); \
		PyThreadState_Swap(c->thread); \
		r = PyObject_CallObject((PyObject *)c->callback, arglist); \
		PyErr_Print(); \
		Py_XDECREF(r); \
		PyThreadState_Swap(NULL); \
		PyEval_ReleaseLock(); \
		g = g->next; \
	}

		//PyEval_AcquireLock();
		//

#define PYCTRLPROXY_MAKESTRUCT(VAR, TYPE) \
	if(self->ptr == NULL) { \
		PyErr_SetString(PyExc_TypeError, "Object is not initialized"); \
		return NULL; \
	} \
	VAR = (struct TYPE *)self->ptr; \
	if(VAR == NULL) { \
		PyErr_SetString(PyExc_TypeError, "Object seems to be deleted"); \
		return NULL; \
	}


/*
#define PYCTRLPROXY_CREATEWRAPPER(NAME,TYPE) \
static PyObject *NAME(void *c) { \
	Py_INCREF(Py_None); \
	return Py_None; \
}
*/
#define PYCTRLPROXY_CREATEWRAPPER(NAME,TYPE) \
static PyObject *NAME(void *c) { \
	PyCtrlproxyObject *rv = (PyCtrlproxyObject *)PyObject_New(PyCtrlproxyObject,&TYPE); \
	rv->ptr = c; \
	return (PyObject *)rv; \
}
// 	rv->ptr = PyCObject_FromVoidPtr(c, &de_alloc);

struct script_thread {
	char *script;
	int	*id;
	PyThreadState *thread;
};

struct callback_object {
	PyObject *callback;
	PyThreadState *thread;
};

struct admin_callback_object {
	PyObject *callback;
	PyThreadState *thread;
	char *name;
};

typedef struct {
    PyObject_HEAD
	long log_level; // we have to use longs here, because python int is a c long
	char *buffer;
} PyCtrlproxyLog;


typedef struct {
	PyObject_HEAD
	void *ptr;
	PyObject *cache;
	PyThreadState *thread;
} PyCtrlproxyEvent;


typedef struct {
	PyObject_HEAD
	void *ptr;
} PyCtrlproxyObject;

typedef struct {
	PyObject_HEAD
	struct transport_ops *ops;
} PyCtrlproxyTransport;

static void PyCtrlproxyObject_dealloc(PyCtrlproxyObject *self)
{
	//printf("DEALLOC ptr %p\n",self->ptr);
    self->ob_type->tp_free((PyObject *)self);
};

// create*Object

static PyObject *createClientObject(void *c);
static PyObject *createNetworkObject(void *c);
static PyObject *createLineObject(void *c);
static PyObject *createNickObject(void *c);
static PyObject *createChannelObject(void *c);


// wrapper object for stdout, stderr -> g_log

static void PyCtrlproxyLog_dealloc(PyCtrlproxyLog *self)
{
	free((int *)self->log_level);
	free(self->buffer);
    self->ob_type->tp_free((PyObject *)self);
};

static int PyCtrlproxyLog_init(PyCtrlproxyLog *self, PyObject *args, PyObject *kwds) {
	static char *kwlist[] = {"loglevel", NULL};
	int *log_level = NULL;

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "|i:__init__", kwlist,
                                      &log_level))
		return -1;
	if (log_level != NULL) {
		self->log_level = (int)log_level;

	}

	self->buffer = "";

	return 0;
};

static PyObject * PyCtrlproxyLog_log(PyCtrlproxyLog *self, PyObject *args, PyObject *kwds){
	static char *kwlist[] = {"loglevel","message", NULL};
	int *log_level = NULL;
	char *msg = NULL;
	char **smsg = NULL;
	int i;

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "|is:log", kwlist,
                                      &log_level,&msg))
		return NULL;

	if(self->buffer == NULL) {
		self->buffer = "";
	}

	self->buffer = g_strconcat(self->buffer,msg,NULL);

	// we only print messages, when they contain a \n
	// this is required for stderr to work correctly
	if(g_strrstr(self->buffer,"\n")) {
		smsg = g_strsplit (self->buffer,"\n",0);
		for(i = 0;smsg[i] != NULL;i++) {
			if(smsg[i] != NULL && strcasecmp(smsg[i],"")) {
				g_log(G_LOG_DOMAIN, (int)log_level, "%s", smsg[i]);
				//printf("%s\n", smsg[i]);
			}
		}
		free(self->buffer);
		g_strfreev(smsg);
		self->buffer = "";
	}
	Py_INCREF(Py_None);
	return Py_None;
};

static PyObject * PyCtrlproxyLog_write(PyCtrlproxyLog *self, PyObject *args, PyObject *kwds)
{
	char *msg = NULL;

    if (! PyArg_ParseTuple(args, "|s:write",
										&msg)) // |z is not unicode save
		return NULL;

	return PyCtrlproxyLog_log(self, Py_BuildValue("(is)",self->log_level,msg), NULL);
	//return rv;
	Py_INCREF(Py_None);
	return Py_None;
};


static PyMethodDef PyCtrlproxyLog_methods[] = {
    {"write", (PyCFunction)PyCtrlproxyLog_write,METH_VARARGS | METH_KEYWORDS, "Write message with the default level"},
    {"log", (PyCFunction)PyCtrlproxyLog_log,METH_VARARGS | METH_KEYWORDS | METH_STATIC, "Write with a specific level"},
	{NULL}                      /* Sentinel */
};

static PyMemberDef PyCtrlproxyLog_members[] = {
    {"level", T_INT, offsetof(PyCtrlproxyLog, log_level), 0, "Current LOG Level"},
	{NULL}                      /* Sentinel */
};



static PyTypeObject PyCtrlproxyLogType = {
    PyObject_HEAD_INIT(NULL)
    0,                          /* ob_size */
    "ctrlproxy.Log",       /* tp_name */
    sizeof(PyCtrlproxyLog), /* tp_basicsize */
    0,                          /* tp_itemsize */
    (destructor)PyCtrlproxyLog_dealloc,/* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_compare */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                         /* tp_call */
    0,                         /* tp_str */
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
    "Ctrlproxy log wrapper", 	/* tp_doc */
    0,                          /* tp_traverse */
    0,                          /* tp_clear */
    0,                          /* tp_richcompare */
    0,                          /* tp_weaklistoffset */
    0,                          /* tp_iter */
    0,                          /* tp_iternext */
    PyCtrlproxyLog_methods,     /* tp_methods */
    PyCtrlproxyLog_members,     /* tp_members */
    0,                          /* tp_getset */
    0,                          /* tp_base */
    0,                          /* tp_dict */
    0,                          /* tp_descr_get */
    0,                          /* tp_descr_set */
    0,                          /* tp_dictoffset */
    (initproc)PyCtrlproxyLog_init, /* tp_init */
    0,                             /* tp_alloc */
    PyType_GenericNew,             /* tp_new */
};

// END OF PyCtrlproxyLog

// NICK

static PyObject * PyCtrlproxyNick_get(PyCtrlproxyObject *self, char *closure) {
	struct channel_nick *n = NULL;

	PYCTRLPROXY_MAKESTRUCT(n, channel_nick)

	if(!strcmp(closure,"name")) {
		return PyString_FromString(n->global->name);
	} else if(!strcmp(closure,"mode")) {
		return PyString_FromString(&n->mode);
	} else if(!strcmp(closure,"hostmask")) {
		if(n->global->hostmask != NULL) {
			return PyString_FromString(n->global->hostmask);
		} else {
			Py_INCREF(Py_None);
			return Py_None;
		}
	} else if(!strcmp(closure,"channel")) {
		return (PyObject *)createChannelObject(n->channel);
	}

}

static int PyCtrlproxyNick_set(PyCtrlproxyObject *self, PyObject *var, void *closure) {
	PyErr_SetString(PyExc_TypeError, "Networks object is readonly"); \
	return -1;
}


static PyMethodDef PyCtrlproxyNick_methods[] = {
	{NULL}                      /* Sentinel */
};


static PyGetSetDef PyCtrlproxyNick_GetSetDef[] = {
    {"name",(getter)PyCtrlproxyNick_get,(setter)PyCtrlproxyNick_get,
	"name of the nick","name"},
    {"channel",(getter)PyCtrlproxyNick_get,(setter)PyCtrlproxyNick_get,
	"channel the nick is on","channel"},
    {"hostmask",(getter)PyCtrlproxyNick_get,(setter)PyCtrlproxyNick_get,
	"hostmask of the user","hostmask"},
    {"mode",(getter)PyCtrlproxyNick_get,(setter)PyCtrlproxyNick_get,
	"mode","mode"},
	{NULL}
};


static PyTypeObject PyCtrlproxyNickType = {
    PyObject_HEAD_INIT(NULL)
    0,                          /* ob_size */
    "ctrlproxy.Nick",       /* tp_name */
    sizeof(PyCtrlproxyObject), /* tp_basicsize */
    0,                          /* tp_itemsize */
    (destructor)PyCtrlproxyObject_dealloc,/* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_compare */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                         /* tp_call */
    0,                         /* tp_str */
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
    "Nick Object", 				/* tp_doc */
    0,                          /* tp_traverse */
    0,                          /* tp_clear */
    0,                          /* tp_richcompare */
    0,                          /* tp_weaklistoffset */
    0,                          /* tp_iter */
    0,                          /* tp_iternext */
    PyCtrlproxyNick_methods,     /* tp_methods */
    0,     						/* tp_members */
    PyCtrlproxyNick_GetSetDef,	/* tp_getset */
    0,                          /* tp_base */
    0,                          /* tp_dict */
    0,                          /* tp_descr_get */
    0,                          /* tp_descr_set */
    0,                          /* tp_dictoffset */
    0, /* tp_init */
    0,                             /* tp_alloc */
    PyType_GenericNew,             /* tp_new */
};

PYCTRLPROXY_CREATEWRAPPER(createNickObject,PyCtrlproxyNickType);



// CHANNEL

static PyObject * PyCtrlproxyChannel_get(PyCtrlproxyObject *self, char *closure) {
	struct channel *c = NULL;
	PyObject *rv = NULL;

	//printf("Channel access at %p\n",self->ptr);

	PYCTRLPROXY_MAKESTRUCT(c, channel)

	if(!strcmp(closure,"xmlConf")) {
		rv = PYCTRLPROXY_NODETOPYOB(c->xmlConf)
	} else if(!strcmp(closure,"name")) {
		rv = PyString_FromString(xmlGetProp(c->xmlConf,"name"));
	} else if(!strcmp(closure,"mode")) {
		rv = PyString_FromString(&c->mode);
	} else if(!strcmp(closure,"modes")) {
		rv = PyString_FromString(c->modes);
	} else if(!strcmp(closure,"introduced")) {
		rv = PyString_FromString(&c->introduced);
	} else if(!strcmp(closure,"namreply_started")) {
		rv = PyString_FromString(&c->namreply_started);
	} else if(!strcmp(closure,"limit")) {
		rv = PyInt_FromLong(c->limit);
	} else if(!strcmp(closure,"nicks")) {
		GList *gl = c->nicks;
		rv = PyDict_New();
		while(gl) {
			struct channel_nick *n = (struct channel_nick *)gl->data;
			PyDict_SetItemString(rv,n->global->name,(PyObject *)createNickObject(gl->data));
			gl = gl->next;
		}
	} else if(!strcmp(closure,"network")) {
		rv = (PyObject *)createNetworkObject(c->network);
	}

	return rv;
}

static int PyCtrlproxyChannel_set(PyCtrlproxyObject *self, PyObject *var, void *closure) {
	PyErr_SetString(PyExc_TypeError, "Networks object is readonly"); \
	return -1;
}


static PyMethodDef PyCtrlproxyChannel_methods[] = {
	{NULL}                      /* Sentinel */
};


static PyGetSetDef PyCtrlproxyChannel_GetSetDef[] = {
	{"xmlConf",(getter)PyCtrlproxyChannel_get,(setter)PyCtrlproxyChannel_set,
	"XML Node of the network","xmlConf"},
    {"name",(getter)PyCtrlproxyChannel_get,(setter)PyCtrlproxyChannel_set,
	"Channel name","name"},
    {"topic",(getter)PyCtrlproxyChannel_get,(setter)PyCtrlproxyChannel_set,
	"Current Topic","topic"},
    {"mode",(getter)PyCtrlproxyChannel_get,(setter)PyCtrlproxyChannel_set,
	"modes","mode"},
    {"modes",(getter)PyCtrlproxyChannel_get,(setter)PyCtrlproxyChannel_set,
	"modes","modes"},
    {"introduced",(getter)PyCtrlproxyChannel_get,(setter)PyCtrlproxyChannel_set,
	"Introduced","introduced"},
    {"namreply_started",(getter)PyCtrlproxyChannel_get,(setter)PyCtrlproxyChannel_set,
	"namreply started","namreply_started"},
    {"limit",(getter)PyCtrlproxyChannel_get,(setter)PyCtrlproxyChannel_set,
	"Channel Limit","limit"},
    {"nicks",(getter)PyCtrlproxyChannel_get,(setter)PyCtrlproxyChannel_set,
	"List of Nicks","nicks"},
    {"network",(getter)PyCtrlproxyChannel_get,(setter)PyCtrlproxyChannel_set,
	"Network","network"},
	{NULL}
};


static PyTypeObject PyCtrlproxyChannelType = {
    PyObject_HEAD_INIT(NULL)
    0,                          /* ob_size */
    "ctrlproxy.Channel",       /* tp_name */
    sizeof(PyCtrlproxyObject), /* tp_basicsize */
    0,                          /* tp_itemsize */
    (destructor)PyCtrlproxyObject_dealloc,/* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_compare */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                         /* tp_call */
    0,                         /* tp_str */
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
    "Channel Object", 			/* tp_doc */
    0,                          /* tp_traverse */
    0,                          /* tp_clear */
    0,                          /* tp_richcompare */
    0,                          /* tp_weaklistoffset */
    0,                          /* tp_iter */
    0,                          /* tp_iternext */
    PyCtrlproxyChannel_methods,     /* tp_methods */
    0,     						/* tp_members */
    PyCtrlproxyChannel_GetSetDef,	/* tp_getset */
    0,                          /* tp_base */
    0,                          /* tp_dict */
    0,                          /* tp_descr_get */
    0,                          /* tp_descr_set */
    0,                          /* tp_dictoffset */
    0, /* tp_init */
    0,                             /* tp_alloc */
    PyType_GenericNew,             /* tp_new */
};


PYCTRLPROXY_CREATEWRAPPER(createChannelObject,PyCtrlproxyChannelType);


// NETWORK

static PyObject * PyCtrlproxyNetwork_get(PyCtrlproxyObject *self, char *closure) {
	struct network *n = NULL;
	PyObject *rv = NULL;

	//printf("Network access at %p\n",self->ptr);

	PYCTRLPROXY_MAKESTRUCT(n, network)

	if(!strcmp(closure,"xmlConf")) {
		rv = PYCTRLPROXY_NODETOPYOB(n->xmlConf)
	} else if(!strcmp(closure,"name")) {
		rv = PyString_FromString(xmlGetProp(n->xmlConf,"name"));
	} else if(!strcmp(closure,"mymodes")) {
		rv = PyString_FromString(n->mymodes);
	} else if(!strcmp(closure,"servers")) {
		rv = PYCTRLPROXY_NODETOPYOB(n->servers)
	} else if(!strcmp(closure,"hostmask")) {
		rv = PyString_FromString(n->hostmask);
	} else if(!strcmp(closure,"channels")) {
		GList *gl = n->channels;
		rv = PyDict_New();
		while(gl) {
			struct channel *c = (struct channel *)gl->data;
			PyDict_SetItemString(rv,xmlGetProp(c->xmlConf, "name"),(PyObject *)createChannelObject(gl->data));
			gl = gl->next;
		}
	} else if(!strcmp(closure,"authenticated")) {
		rv = PyString_FromString(&n->authenticated);
	} else if(!strcmp(closure,"clients")) {
		GList *gl = n->channels;
		rv = PyList_New(0);
		while(gl) {
			struct client *c = (struct client *)gl->data;
			//PyList_Append(createChannel(
			PyList_Append(rv,(PyObject *)createClientObject(gl->data));
			gl = gl->next;
		}
	} else if(!strcmp(closure,"current_server")) {
		rv = PYCTRLPROXY_NODETOPYOB(n->current_server)
	} else if(!strcmp(closure,"listen")) {
		rv = PYCTRLPROXY_NODETOPYOB(n->listen)
	} else if(!strcmp(closure,"supported_modes")) {
		if(n->supported_modes != NULL) {
			rv = PyList_New(0);
			if(n->supported_modes[0] != NULL)
				PyList_Append(rv,PyString_FromFormat("%s",n->supported_modes[0]));
			else
				PyList_Append(rv,PyString_FromString("aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ"));
			if(n->supported_modes[1] != NULL)
				PyList_Append(rv,PyString_FromFormat("%s",n->supported_modes[1]));
			else
				PyList_Append(rv,PyString_FromString("aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ"));
		}
	} else if(!strcmp(closure,"features")) {
		int i = 0;
		rv = PyList_New(0);
		if(n->features != NULL) {
			for(i = 0; n->features[i]; i++) {
				PyList_Append(rv,PyString_FromString(n->features[i]));
			}
		}
	}

	if(rv == NULL) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	return rv;


}
static int PyCtrlproxyNetwork_set(PyCtrlproxyObject *self, PyObject *var, void *closure) {
	PyErr_SetString(PyExc_TypeError, "Networks object is readonly"); \
	return -1;
}


static PyObject * PyCtrlproxyNetwork_send(PyCtrlproxyObject *self, PyObject *args, PyObject *kwds) {
	static char *kwlist[] = {"msg","direction",NULL};
	char *msg;
	int dir = TO_SERVER;
	int i = 0;
	struct network *n;

	PYCTRLPROXY_MAKESTRUCT(n, network)

	if (! PyArg_ParseTupleAndKeywords(args, kwds, "s|i:send", kwlist,
                                      &msg,&dir))
		return NULL;

	if (dir == FROM_SERVER) {
		for(i = 0;n->incoming[i] != NULL;i++) {
			transport_write(n->incoming[i], msg);
		}
	} else if(dir == TO_SERVER) {
		transport_write(n->outgoing, msg);
	} else {
		PyErr_SetString(PyExc_ValueError, "direction unknown");
		return NULL;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/*
static PyObject * PyCtrlproxyNetwork_connect(PyCtrlproxyObject *self, PyObject *args, PyObject *kwds) {
	static char *kwlist[] = {"msg","direction",NULL};
	char *msg;
	int dir = TO_SERVER;
	int i = 0;
	struct network *n;

	PYCTRLPROXY_MAKESTRUCT(n, network)

	connect_server(n->xmlConf);

	Py_INCREF(Py_None);
	return Py_None;
}
*/

static PyObject * PyCtrlproxyNetwork_disconnect(PyCtrlproxyObject *self, PyObject *args, PyObject *kwds) {
	static char *kwlist[] = {"msg","direction",NULL};
	char *msg;
	int dir = TO_SERVER;
	int i = 0;
	struct network *n;

	PYCTRLPROXY_MAKESTRUCT(n, network)

	close_network(n);

	// we are now invalide :)
	self->ptr = NULL;

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject * PyCtrlproxyNetwork_add_listen(PyCtrlproxyObject *self, PyObject *args, PyObject *kwds) {
	static char *kwlist[] = {"msg","direction",NULL};
	char *msg;
	int dir = TO_SERVER;
	int i = 0;
	PyObject *listen = NULL;
	struct network *n;

	PYCTRLPROXY_MAKESTRUCT(n, network)

	if (! PyArg_ParseTupleAndKeywords(args, kwds, "O:add_listen", kwlist,
                                      &listen))
		return NULL;

	if(! PyObject_IsSubclass(listen, xml_node)) {
		PyErr_SetString(PyExc_TypeError, "Parameter is not a xmlNodeObject");
		return NULL;
	}

	network_add_listen(n, PyxmlNode_Get(listen));
	Py_INCREF(Py_None);
	return Py_None;

}

static PyMethodDef PyCtrlproxyNetwork_methods[] = {
    {"send", (PyCFunction)PyCtrlproxyNetwork_send,METH_VARARGS | METH_KEYWORDS, "Write message into the network"},
//	{"connect", (PyCFunction)PyCtrlproxyNetwork_connect, METH_NOARGS, "Connect to a server"},
	{"disconnect", (PyCFunction)PyCtrlproxyNetwork_disconnect, METH_NOARGS, "Disconnect from server in this network"},
    {"add_listen", (PyCFunction)PyCtrlproxyNetwork_add_listen,METH_VARARGS | METH_KEYWORDS, "Add a listen port for this network"},
	{NULL}                      /* Sentinel */
};


static PyGetSetDef PyCtrlproxyNetwork_GetSetDef[] = {
    {"xmlConf",(getter)PyCtrlproxyNetwork_get,(setter)PyCtrlproxyNetwork_set,
	"XML Node of the network","xmlConf"},
    {"name",(getter)PyCtrlproxyNetwork_get,(setter)PyCtrlproxyNetwork_set,
	"Name of network","name"},
    {"mymodes",(getter)PyCtrlproxyNetwork_get,(setter)PyCtrlproxyNetwork_set,
	"my modes","mymodes"},
    {"servers",(getter)PyCtrlproxyNetwork_get,(setter)PyCtrlproxyNetwork_set,
	"XML Node of servers","servers"},
    {"hostmask",(getter)PyCtrlproxyNetwork_get,(setter)PyCtrlproxyNetwork_set,
	"current hostmask","hostmask"},
    {"channels",(getter)PyCtrlproxyNetwork_get,(setter)PyCtrlproxyNetwork_set,
	"List of channels","channels"},
    {"authenticated",(getter)PyCtrlproxyNetwork_get,(setter)PyCtrlproxyNetwork_set,
	"authenticated","authenticated"},
    {"clients",(getter)PyCtrlproxyNetwork_get,(setter)PyCtrlproxyNetwork_set,
	"List of Clients","clients"},
    {"current_server",(getter)PyCtrlproxyNetwork_get,(setter)PyCtrlproxyNetwork_set,
	"XML Node of current server","current_server"},
    {"listen",(getter)PyCtrlproxyNetwork_get,(setter)PyCtrlproxyNetwork_set,
	"XML Node of listen interface","listen"},
    {"supported_modes",(getter)PyCtrlproxyNetwork_get,(setter)PyCtrlproxyNetwork_set,
	"supported modes","supported_modes"},
    {"features",(getter)PyCtrlproxyNetwork_get,(setter)PyCtrlproxyNetwork_set,
	"features provided by server","features"},
/*    {"outgoing",(getter)PyCtrlproxyNetwork_get,(setter)PyCtrlproxyNetwork_set,
	"Outgoing Transport Object","outgoing"},
    {"incoming",(getter)PyCtrlproxyNetwork_get,(setter)PyCtrlproxyNetwork_set,
	"Incoming Transport Object","incoming"},
*/	{NULL}
};




static PyTypeObject PyCtrlproxyNetworkType = {
    PyObject_HEAD_INIT(NULL)
    0,                          /* ob_size */
    "ctrlproxy.Network",       /* tp_name */
    sizeof(PyCtrlproxyObject), /* tp_basicsize */
    0,                          /* tp_itemsize */
    (destructor)PyCtrlproxyObject_dealloc,/* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_compare */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                         /* tp_call */
    0,                         /* tp_str */
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
    "Network Object", 	/* tp_doc */
    0,                          /* tp_traverse */
    0,                          /* tp_clear */
    0,                          /* tp_richcompare */
    0,                          /* tp_weaklistoffset */
    0,                          /* tp_iter */
    0,                          /* tp_iternext */
    PyCtrlproxyNetwork_methods,     /* tp_methods */
    0,     /* tp_members */
    PyCtrlproxyNetwork_GetSetDef,                          /* tp_getset */
    0,                          /* tp_base */
    0,                          /* tp_dict */
    0,                          /* tp_descr_get */
    0,                          /* tp_descr_set */
    0,                          /* tp_dictoffset */
    0, /* tp_init */
    0,                             /* tp_alloc */
    PyType_GenericNew,             /* tp_new */
};


PYCTRLPROXY_CREATEWRAPPER(createNetworkObject,PyCtrlproxyNetworkType);

// CLIENT

static PyObject * PyCtrlproxyClient_get(PyCtrlproxyObject *self, void *closure) {
	struct client *c = NULL;
	PyObject *rv = NULL;

	//printf("Client access at %p\n",self->ptr);

	PYCTRLPROXY_MAKESTRUCT(c, client)

	if(!strcmp(closure,"network")) {
		rv = (PyObject *)createNetworkObject(c->network);
	} else if(!strcmp(closure,"transport_context")) {
		//rv = PyString_FromString(l->authenticated);
	} else if(!strcmp(closure,"connect_time")) {
		rv = PyInt_FromLong((long)c->connect_time);
	} else if(!strcmp(closure,"did_nick_change")) {
		rv = PyString_FromString(&c->did_nick_change);
	} else if(!strcmp(closure,"nick")) {
		rv = PyString_FromString(xmlGetProp(c->network->xmlConf, "nick"));
	}
	return rv;
}

static PyObject * PyCtrlproxyClient_send(PyCtrlproxyObject *self, PyObject *args, PyObject *kwds) {
	static char *kwlist[] = {"msg",NULL};
	char *msg;
	struct client *c = NULL;

	//printf("Client send at %p\n",self->ptr);

	PYCTRLPROXY_MAKESTRUCT(c, client)

	if (! PyArg_ParseTupleAndKeywords(args, kwds, "s:send", kwlist,
                                      &msg))
		return NULL;

	if(c->incoming == NULL) {
		PyErr_SetString(PyExc_TypeError, "Not writeable");
		return NULL;
	}
	transport_write(c->incoming, msg);

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject * PyCtrlproxyClient_sendNotice(PyCtrlproxyObject *self, PyObject *args, PyObject *kwds) {
	static char *kwlist[] = {"msg",NULL};
	char *msg, *nick, *tot, *server_name;
	PyObject *rv = NULL;
	struct client *c = NULL;

	PYCTRLPROXY_MAKESTRUCT(c, client)

	if (! PyArg_ParseTupleAndKeywords(args, kwds, "s:sendNotice", kwlist,
                                      &msg))
		return NULL;

	if(c->network == NULL) {
		PyErr_SetString(PyExc_TypeError, "No Network available");
		return NULL;
	}
	printf("XM: %p\n",c->network->xmlConf);
	//xmlDebugDumpNode(stdout,l->network->xmlConf,0);
	nick = xmlGetProp(c->network->xmlConf, "nick");
	server_name = xmlGetProp(c->network->xmlConf, "name");

	asprintf(&tot, ":ctrlproxy!ctrlproxy@%s NOTICE %s :%s\r\n", server_name, nick, msg);
	rv = PyCtrlproxyClient_send(self, Py_BuildValue("(s)",tot),NULL);

	xmlFree(nick);
	xmlFree(server_name);
	return rv;
}

static PyObject * PyCtrlproxyClient_disconnect(PyCtrlproxyObject *self, PyObject *args, PyObject *kwds) {
	char *msg, *nick, *tot, *server_name;
	struct client *c = NULL;

	PYCTRLPROXY_MAKESTRUCT(c, client)

	disconnect_client(c);

	Py_INCREF(Py_None);
	return Py_None;
}


static PyMethodDef PyCtrlproxyClient_methods[] = {
    {"send", (PyCFunction)PyCtrlproxyClient_send,METH_VARARGS | METH_KEYWORDS, "Sends a raw string to the client"},
    {"send_notice", (PyCFunction)PyCtrlproxyClient_sendNotice,METH_VARARGS | METH_KEYWORDS, ""},
	{"disconnect", (PyCFunction)PyCtrlproxyClient_disconnect,METH_NOARGS, ""},
	{NULL}                      /* Sentinel */
};


static int PyCtrlproxyClient_set(PyCtrlproxyObject *self, PyObject *var, void *closure) {
	PyErr_SetString(PyExc_TypeError, "Client object is readonly"); \
	return -1;
}

static PyGetSetDef PyCtrlproxyClient_GetSetDef[] = {
    {"network",(getter)PyCtrlproxyClient_get,(setter)PyCtrlproxyClient_set,
	"Network Object","network"},
    {"connect_time",(getter)PyCtrlproxyClient_get,(setter)PyCtrlproxyClient_set,
	"Time when the client connected","connect_time"},
    {"did_nick_change",(getter)PyCtrlproxyClient_get,(setter)PyCtrlproxyClient_set,
	"did_nick_change","did_nick_change"},
    {"nick",(getter)PyCtrlproxyClient_get,(setter)PyCtrlproxyClient_set,
	"Nickname","nick"},
	{NULL}
};

static PyTypeObject PyCtrlproxyClientType = {
    PyObject_HEAD_INIT(NULL)
    0,                          /* ob_size */
    "ctrlproxy.Client",       /* tp_name */
    sizeof(PyCtrlproxyObject), /* tp_basicsize */
    0,                          /* tp_itemsize */
    (destructor)PyCtrlproxyObject_dealloc,/* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_compare */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                         /* tp_call */
    0,                         /* tp_str */
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
    "Client Object", 	/* tp_doc */
    0,                          /* tp_traverse */
    0,                          /* tp_clear */
    0,                          /* tp_richcompare */
    0,                          /* tp_weaklistoffset */
    0,                          /* tp_iter */
    0,                          /* tp_iternext */
    PyCtrlproxyClient_methods,     /* tp_methods */
    0,     /* tp_members */
    PyCtrlproxyClient_GetSetDef,                          /* tp_getset */
    0,                          /* tp_base */
    0,                          /* tp_dict */
    0,                          /* tp_descr_get */
    0,                          /* tp_descr_set */
    0,                          /* tp_dictoffset */
    0, /* tp_init */
    0,                             /* tp_alloc */
    PyType_GenericNew,             /* tp_new */
};


PYCTRLPROXY_CREATEWRAPPER(createClientObject,PyCtrlproxyClientType);

// LINE

static PyObject * PyCtrlproxyLine_get(PyCtrlproxyObject *self, void *closure) {
	struct line *l = NULL;
	PyObject *rv = NULL;
	int i = 0;

	PYCTRLPROXY_MAKESTRUCT(l, line)

	if(!strcmp(closure,"direction")) {
		rv = PyInt_FromLong((long)l->direction);
	} else if(!strcmp(closure,"options")) {
		rv = PyInt_FromLong((long)l->options);
	} else if(!strcmp(closure,"network")) {
		rv = (PyObject *)createNetworkObject(l->network);
	} else if(!strcmp(closure,"client")) {
		if(FROM_SERVER == l->direction) {
			PyErr_SetString(PyExc_TypeError, "Line comes from Server and has no Client");
			return NULL;
		} else {
			rv = (PyObject *)createClientObject(l->client);
		}
	} else if(!strcmp(closure,"origin")) {
		if (l->origin != NULL)
			rv = PyString_FromString(l->origin);
		else {
			Py_INCREF(Py_None);
			rv = Py_None;
		}
	} else if(!strcmp(closure,"args")) {
		rv = PyList_New(0);
		for(i = 0; i < l->argc; i++) {
			if(l->args[i] == NULL) {
				Py_INCREF(Py_None);
				PyList_Append(rv,Py_None);
			} else
				PyList_Append(rv,PyString_FromFormat("%s",l->args[i]));
		}
	}
	//Py_XINCREF(rv);
	return rv;
}

static int PyCtrlproxyLine_set(PyCtrlproxyObject *self, PyObject *var, void *closure) {
	struct line *l = NULL;

	if(self->ptr == NULL) {
		PyErr_SetString(PyExc_TypeError, "Object is not initialized");
		return -1;
	}
	l = (struct line *)self->ptr;
	if(l == NULL) {
		PyErr_SetString(PyExc_TypeError, "Object seems to be deleted"); \
		return -1;
	}

	//PYCTRLPROXY_MAKESTRUCT(l, line)

	if(!strcmp(closure,"options")) {
		if(!PyInt_Check(var)) {
			PyErr_SetString(PyExc_ValueError, "Var is no int");
			return -1;
		}
		l->options = (int)PyInt_AsLong(var);
	} else {
		PyErr_SetString(PyExc_TypeError, "Option is readonly");
	}

	return 0;
}

static PyGetSetDef PyCtrlproxyLine_GetSetDef[] = {
    {"direction",(getter)PyCtrlproxyLine_get,(setter)PyCtrlproxyLine_set,
	"Direction of line","direction"},
    {"options",(getter)PyCtrlproxyLine_get,(setter)PyCtrlproxyLine_set,
	"Options","options"},
    {"network",(getter)PyCtrlproxyLine_get,(setter)PyCtrlproxyLine_set,
	"Network to which the Line resists","network"},
    {"client",(getter)PyCtrlproxyLine_get,(setter)PyCtrlproxyLine_set,
	"Client Object","client"},
    {"origin",(getter)PyCtrlproxyLine_get,(setter)PyCtrlproxyLine_set,
	"","origin"},
    {"args",(getter)PyCtrlproxyLine_get,(setter)PyCtrlproxyLine_set,
	"Arguments","args"},
	{NULL}
};

static PyTypeObject PyCtrlproxyLineType = {
    PyObject_HEAD_INIT(NULL)
    0,                          /* ob_size */
    "ctrlproxy.Line",       /* tp_name */
    sizeof(PyCtrlproxyObject), /* tp_basicsize */
    0,                          /* tp_itemsize */
    (destructor)PyCtrlproxyObject_dealloc,/* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_compare */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                         /* tp_call */
    0,                         /* tp_str */
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
    "Line Object", 	/* tp_doc */
    0,                          /* tp_traverse */
    0,                          /* tp_clear */
    0,                          /* tp_richcompare */
    0,                          /* tp_weaklistoffset */
    0,                          /* tp_iter */
    0,                          /* tp_iternext */
    0,     /* tp_methods */
    0,     /* tp_members */
    PyCtrlproxyLine_GetSetDef,                          /* tp_getset */
    0,                          /* tp_base */
    0,                          /* tp_dict */
    0,                          /* tp_descr_get */
    0,                          /* tp_descr_set */
    0,                          /* tp_dictoffset */
    0, /* tp_init */
    0,                             /* tp_alloc */
    PyType_GenericNew,             /* tp_new */
};


PYCTRLPROXY_CREATEWRAPPER(createLineObject,PyCtrlproxyLineType);

// Event Object

static void PyCtrlproxyEvent_dealloc(PyCtrlproxyLog *self)
{
    self->ob_type->tp_free((PyObject *)self);
};


static int PyCtrlproxyEvent_init(PyCtrlproxyLog *self, PyObject *args, PyObject *kwds) {
	int *log_level = NULL;

    if (! PyArg_ParseTuple(args, "|:__init__"))
		return -1;

	return 0;
};

#define PYCTRLPROXY_ADDLINECACHE() \
		nv = PyList_New(0); \
		i = 0; \
		for(; l->args[i] != NULL; i++) \
			PyList_Append(nv,PyString_FromString(l->args[i])); \
		PyList_Append(cur,nv); \

#define PYCTRLPROXY_CALLEVENT(NAME) \
		printf("CALL %s\n",NAME); \
		if(PyObject_HasAttrString((PyObject *)self, NAME)) { \
			r = PyObject_CallMethod((PyObject *)self, NAME, "(OO)", createLineObject(l),cur); \
			PyErr_Print(); \
			Py_XDECREF(r); \
		 } \
		PyDict_DelItemString(self->cache,net);

#define PYCTRLPROXY_INITCUR() \
	if(PyDict_GetItemString(self->cache,net) == NULL) \
		PyDict_SetItemString(self->cache,net,PyList_New(0)); \
	cur = PyDict_GetItemString(self->cache,net);

// direct implementation
static void PyCtrlproxyEvent_catch(PyCtrlproxyEvent *self, struct line *l){
	PyObject *nv = NULL;
	PyObject *cur = NULL;
	PyObject *all = PyList_New(0);
	PyObject *r = NULL;
	int i = 0;
	char *net = xmlGetProp(l->network->xmlConf, "name");

	if(l->args[0] == NULL) {
		free(net);
		return;
	}
	if(self->cache == NULL)
		self->cache = PyDict_New();

	PYCTRLPROXY_INITCUR()

	if(!strcasecmp(l->args[0],"TOPIC")) {
			PYCTRLPROXY_ADDLINECACHE();
			PYCTRLPROXY_CALLEVENT("onTopicChange")
	} else if(!strcasecmp(l->args[0],"JOIN")) {
			PYCTRLPROXY_ADDLINECACHE();
			PYCTRLPROXY_CALLEVENT("onChannelJoin")
	} else if(!strcasecmp(l->args[0],"PART")) {
			PYCTRLPROXY_ADDLINECACHE();
			PYCTRLPROXY_CALLEVENT("onChannelPart")
	} else if(!strcasecmp(l->args[0],"INVITE")) {
			PYCTRLPROXY_ADDLINECACHE();
			PYCTRLPROXY_CALLEVENT("onInvite")
	} else if(!strcasecmp(l->args[0],"MODE")) {
			PYCTRLPROXY_ADDLINECACHE();
			PYCTRLPROXY_CALLEVENT("onMode")
	} else if(!strcasecmp(l->args[0],"MODE") && l->args[2] != NULL && !strcasecmp(l->args[2],"+v")) {
			PYCTRLPROXY_ADDLINECACHE();
			PYCTRLPROXY_CALLEVENT("onVoice")
	} else if(!strcasecmp(l->args[0],"MODE") && l->args[2] != NULL && !strcasecmp(l->args[2],"-v")) {
			PYCTRLPROXY_ADDLINECACHE();
			PYCTRLPROXY_CALLEVENT("onDeVoice")
	} else if(!strcasecmp(l->args[0],"MODE") && l->args[2] != NULL && !strcasecmp(l->args[2],"+o")) {
			PYCTRLPROXY_ADDLINECACHE();
			PYCTRLPROXY_CALLEVENT("onOp")
	} else if(!strcasecmp(l->args[0],"MODE") && l->args[2] != NULL && !strcasecmp(l->args[2],"-o")) {
			PYCTRLPROXY_ADDLINECACHE();
			PYCTRLPROXY_CALLEVENT("onDeOp")
	} else if(!strcasecmp(l->args[0],"MODE") && l->args[2] != NULL && !strcasecmp(l->args[2],"+b")) {
			PYCTRLPROXY_ADDLINECACHE();
			PYCTRLPROXY_CALLEVENT("onBan")
	} else if(!strcasecmp(l->args[0],"MODE") && l->args[2] != NULL && !strcasecmp(l->args[2],"-b")) {
			PYCTRLPROXY_ADDLINECACHE();
			PYCTRLPROXY_CALLEVENT("onUnBan")
	} else if(!strcasecmp(l->args[0],"KICK")) {
			PYCTRLPROXY_ADDLINECACHE();
			PYCTRLPROXY_CALLEVENT("onKick")
	} else if(!strcasecmp(l->args[0],"NOTICE")) {
			PYCTRLPROXY_ADDLINECACHE();
			PYCTRLPROXY_CALLEVENT("onNotice")
	} else if(!strcasecmp(l->args[0],"PING")) {
			PYCTRLPROXY_ADDLINECACHE();
			PYCTRLPROXY_CALLEVENT("onPing")
	} else if(!strcasecmp(l->args[0],"PONG")) {
			PYCTRLPROXY_ADDLINECACHE();
			PYCTRLPROXY_CALLEVENT("onPong")
	} else if(!strcasecmp(l->args[0],"PRIVMSG")) {
			PYCTRLPROXY_ADDLINECACHE();
			PYCTRLPROXY_CALLEVENT("onPrivmsg")
	} else {
		switch(atoi(l->args[0])) {
			// WHOIS & WHOWAS
			case RPL_WHOWASUSER:
			case RPL_WHOISUSER:
			case RPL_WHOISSERVER:
			case RPL_WHOISOPERATOR:
			case RPL_WHOISCHANNELS:
			case RPL_WHOISIDLE:
				PYCTRLPROXY_ADDLINECACHE();
				break;
				// call and clean
			case RPL_ENDOFWHOIS:
				PYCTRLPROXY_CALLEVENT("onWhois")
				break;
			case RPL_ENDOFWHOWAS:
				PYCTRLPROXY_CALLEVENT("onWhowas")
				break;
			case RPL_WHOREPLY:
				PYCTRLPROXY_ADDLINECACHE();
				break;
			case RPL_ENDOFWHO:
				PYCTRLPROXY_CALLEVENT("onWho")
				break;
			// LISTS
			case RPL_LISTSTART:
				cur = PyList_New(0);
			case RPL_LIST:
				PYCTRLPROXY_ADDLINECACHE();
				break;
			case RPL_LISTEND:
				PYCTRLPROXY_CALLEVENT("onList")
				break;
			case RPL_NAMREPLY:
				PYCTRLPROXY_ADDLINECACHE();
				break;
			case RPL_ENDOFNAMES:
				PYCTRLPROXY_ADDLINECACHE();
				PYCTRLPROXY_CALLEVENT("onNames")
				break;
			case RPL_LINKS:
				PYCTRLPROXY_ADDLINECACHE();
			case RPL_ENDOFLINKS:
				PYCTRLPROXY_CALLEVENT("onList")
				break;
			case RPL_CHANNELMODEIS:
				PYCTRLPROXY_CALLEVENT("onChannelMode")
				break;
			// error handling
			case ERR_NOSUCHNICK:
			case ERR_NOSUCHSERVER:
			case ERR_NOSUCHCHANNEL:
			case ERR_CANNOTSENDTOCHAN:
			case ERR_TOOMANYCHANNELS:
			case ERR_WASNOSUCHNICK:
			case ERR_TOOMANYTARGETS:
			case ERR_NOORIGIN:
			case ERR_NORECIPIENT:
			case ERR_NOTEXTTOSEND:
			case ERR_NOTOPLEVEL:
			case ERR_WILDTOPLEVEL:
			case ERR_UNKNOWNCOMMAND:
			case ERR_NOMOTD:
			case ERR_NOADMININFO:
			case ERR_FILEERROR:
			case ERR_NONICKNAMEGIVEN:
			case ERR_ERRONEUSNICKNAME:
			case ERR_NICKNAMEINUSE:
			case ERR_NICKCOLLISION:
			case ERR_USERNOTINCHANNEL:
			case ERR_NOTONCHANNEL:
			case ERR_USERONCHANNEL:
			case ERR_NOLOGIN:
			case ERR_SUMMONDISABLED:
			case ERR_USERSDISABLED:
			case ERR_NOTREGISTERED:
			case ERR_NEEDMOREPARAMS:
			case ERR_ALREADYREGISTRED:
			case ERR_NOPERMFORHOST:
			case ERR_PASSWDMISMATCH:
			case ERR_YOUREBANNEDCREEP:
			case ERR_KEYSET:
			case ERR_CHANNELISFULL:
			case ERR_UNKNOWNMODE:
			case ERR_INVITEONLYCHAN:
			case ERR_BANNEDFROMCHAN:
			case ERR_BADCHANNELKEY:
			case ERR_NOPRIVILEGES:
			case ERR_CHANOPRIVSNEEDED:
			case ERR_CANTKILLSERVER:
			case ERR_NOOPERHOST:
			case ERR_UMODEUNKNOWNFLAG:
			case ERR_USERSDONTMATCH:
				// here some more specific errors
				switch(atoi(l->args[0])) {
					case ERR_NORECIPIENT:
					case ERR_NOTEXTTOSEND:
					case ERR_CANNOTSENDTOCHAN:
					case ERR_NOTOPLEVEL:
					case ERR_WILDTOPLEVEL:
					case ERR_TOOMANYTARGETS:
					case ERR_NOSUCHNICK:
						PYCTRLPROXY_ADDLINECACHE();
						PYCTRLPROXY_CALLEVENT("onPrivmsgError")
					case ERR_BANNEDFROMCHAN:
					case ERR_INVITEONLYCHAN:
					case ERR_BADCHANNELKEY:
					case ERR_CHANNELISFULL:
					case ERR_BADCHANMASK:
					case ERR_TOOMANYCHANNELS:
						PYCTRLPROXY_ADDLINECACHE();
						PYCTRLPROXY_CALLEVENT("onJoinError")
					case ERR_CHANOPRIVSNEEDED:
					case ERR_NOTONCHANNEL:
					case ERR_NOSUCHCHANNEL:
						PYCTRLPROXY_ADDLINECACHE();
						PYCTRLPROXY_CALLEVENT("onChannelError")
						break;
					case ERR_UNKNOWNMODE:
						PYCTRLPROXY_ADDLINECACHE();
						PYCTRLPROXY_CALLEVENT("onModeError")
						break;
				}
				// everything goes to onError
				PYCTRLPROXY_INITCUR()
				PYCTRLPROXY_ADDLINECACHE();
				PYCTRLPROXY_CALLEVENT("onError")
				break;
			default:
				if(strcmp(l->args[0],"")) {
					PYCTRLPROXY_ADDLINECACHE();
					PYCTRLPROXY_CALLEVENT("onElse")
				}
		}
	}
	// call onRcv
	//cur = PyDict_GetItemString(self->cache,net);
	if(PyObject_HasAttrString((PyObject *)self, "onRcv")) {
		for(i = 0; l->args[i] != NULL; i++)
			PyList_Append(all,PyString_FromString(l->args[i]));
		r = PyObject_CallMethod((PyObject *)self, "onRcv", "(OO)", createLineObject(l),all);
		PyErr_Print();
		Py_XDECREF(r);
	}

	free(net);
}


static PyObject * PyCtrlproxyEvent_noop(PyCtrlproxyEvent *self, PyObject *args, PyObject *kwds)
{
	Py_INCREF(Py_None);
	return Py_None;
};


static PyMethodDef PyCtrlproxyEvent_methods[] = {
    {"onJoin", (PyCFunction)PyCtrlproxyEvent_noop,METH_VARARGS | METH_KEYWORDS, "When joining a channel"},
    {"onPart", (PyCFunction)PyCtrlproxyEvent_noop,METH_VARARGS | METH_KEYWORDS | METH_STATIC, "When parting a channel"},
	{NULL}                      /* Sentinel */
};

static PyTypeObject PyCtrlproxyEventType = {
    PyObject_HEAD_INIT(NULL)
    0,                          /* ob_size */
    "ctrlproxy.Event",       /* tp_name */
    sizeof(PyCtrlproxyEvent), /* tp_basicsize */
    0,                          /* tp_itemsize */
    (destructor)PyCtrlproxyEvent_dealloc,/* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_compare */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                         /* tp_call */
    0,                         /* tp_str */
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
    "High Event Object", 	/* tp_doc */
    0,                          /* tp_traverse */
    0,                          /* tp_clear */
    0,                          /* tp_richcompare */
    0,                          /* tp_weaklistoffset */
    0,                          /* tp_iter */
    0,                          /* tp_iternext */
    PyCtrlproxyEvent_methods,     /* tp_methods */
    0,						     /* tp_members */
    0,                          /* tp_getset */
    0,                          /* tp_base */
    0,                          /* tp_dict */
    0,                          /* tp_descr_get */
    0,                          /* tp_descr_set */
    0,                          /* tp_dictoffset */
    (initproc)PyCtrlproxyEvent_init, /* tp_init */
    0,                             /* tp_alloc */
    PyType_GenericNew,             /* tp_new */
};

// TRANSPORT OBJECT

static void PyCtrlproxyTransport_dealloc(PyCtrlproxyTransport *self)
{
	//free((int *)self->log_level);
	//free(self->buffer);
    self->ob_type->tp_free((PyObject *)self);
};

static int PyCtrlproxyTransport_init(PyCtrlproxyTransport *self, PyObject *args, PyObject *kwds) {
	static char *kwlist[] = {"loglevel", NULL};
	int *log_level = NULL;

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "|i:__init__", kwlist,
                                      &log_level))
		return -1;

	return 0;
};




// FUNCTIONS


static void list_scripts(struct line *l)
{
	GList *g = loaded_scripts;
	if(l != NULL)
		admin_out(l, "List scripts");
	printf("List Scripts\n");
	while(g) {
		struct script_thread *p = (struct script_thread *)g->data;
		if(l != NULL)
			admin_out(l, "#%i: %s",p->id,p->script);
		printf("ID:%i:%s\n",(int)p->id,p->script);
		g = g->next;
	}
}



// recv hooks

PYCTRLPROXY_PYADDCALLBACK(PyCtrlproxy_add_rcv_data_hook,py_rcv_hooks)


static gboolean in_rcv_data(struct line *l) {

	//xmlDebugDumpNode(stdout,l->network->xmlConf,0);
	PYCTRLPROXY_PYCALLPYCALLBACKS(py_rcv_hooks,createLineObject)

	// call event object
	g = py_events;
	while(g) {
		struct callback_object *c = (struct callback_object *)g->data;
		PyEval_AcquireLock();
		PyThreadState_Swap(c->thread);
		PyCtrlproxyEvent_catch((PyCtrlproxyEvent *)c->callback,l);
		PyThreadState_Swap(NULL);
		PyEval_ReleaseLock();
		g = g->next;
	}

/*	if(l->direction == TO_SERVER && !strcasecmp(l->args[0], "PRIVMSG") &&
	   	strcasecmp("!PYSHELL",l->args[1]) == 0) {
		l->options |= LINE_DONT_SEND | LINE_IS_PRIVATE;
		printf("PYSHELLSUPPORT\n");
	}
*/
	return TRUE;
}

static PyObject * PyCtrlproxy_del_rcv_data_hook(PyObject *self, PyObject *args, PyObject *keywds) {
    char *command;

    if (!PyArg_ParseTuple(args, "s", &command))
        return NULL;
	Py_INCREF(Py_None);
    return Py_None;
}


// new client hooks

PYCTRLPROXY_PYADDCALLBACK(PyCtrlproxy_add_new_client_hook,py_new_client_hooks)

static gboolean in_new_client_hook(struct client *l) {
	PYCTRLPROXY_PYCALLPYCALLBACKS(py_new_client_hooks,createClientObject)
	return 1;
}




// lose client hooks

PYCTRLPROXY_PYADDCALLBACK(PyCtrlproxy_add_lose_client_hook,py_lose_client_hooks)

static void in_lose_client_hook(struct client *l)
{
	PYCTRLPROXY_PYCALLPYCALLBACKS(py_lose_client_hooks,createClientObject)
}


// motd hooks

PYCTRLPROXY_PYADDCALLBACK(PyCtrlproxy_add_motd_hook,py_motd_hooks)

char ** in_motd_hook(struct network *n)
{
	char **rv = NULL;
	GList *g = py_motd_hooks;
	PyObject *r = NULL;
	int i = 0;
	size_t nrlines = 0;

	while(g) {
		struct callback_object *c = (struct callback_object *)g->data;
		PyObject *lo = createNetworkObject(n);
		PyObject *arglist = Py_BuildValue("(O)", lo);
		PyEval_AcquireLock();
		PyThreadState_Swap(c->thread);
		r = PyObject_CallObject((PyObject *)c->callback, arglist);
		if(!PyList_Check(r)) {
			PyErr_SetString(PyExc_ValueError, "Return Value wrong");
			PyErr_Print();
		} else {
			for(i = 0; i < PyList_Size(r); i++) {
				realloc(rv, (nrlines+2) * sizeof(char *));
				printf("%s\n",PyString_AsString(PyObject_Str(PyList_GetItem(r,i))));
				nrlines++;
				//rv[nrlines] = NULL;
			}
			realloc(rv, (nrlines+2) * sizeof(char *));
			//rv[nrlines] = strdup("\r\n");
			nrlines++;
			//rv[nrlines] = NULL;
		}
		Py_XDECREF(r);
		PyThreadState_Swap(NULL);
		PyEval_ReleaseLock();
		g = g->next;
	}

	return rv;
}


// server connect hook

PYCTRLPROXY_PYADDCALLBACK(PyCtrlproxy_add_server_connected_hook,py_server_connected_hooks)

static void in_server_connected_hook(struct network *l) {
	PYCTRLPROXY_PYCALLPYCALLBACKS(py_server_connected_hooks,createNetworkObject)
}

// server connect hook

PYCTRLPROXY_PYADDCALLBACK(PyCtrlproxy_add_server_disconnected_hook,py_server_disconnected_hooks)

static void in_server_disconnected_hook(struct network *l) {
	PYCTRLPROXY_PYCALLPYCALLBACKS(py_server_disconnected_hooks,createNetworkObject)
}


static PyObject * PyCtrlproxy_list_networks(PyObject *self, PyObject *args, PyObject *kwds){
	PyObject *rv = PyList_New(0);
	GList *l = networks;
	while(l) {
		struct network *s = (struct network *)l->data;
		PyList_Append(rv,PyString_FromString(xmlGetProp(s->xmlConf, "name")));
		l = g_list_next(l);
	}
	//Py_INCREF(rv);
	return rv;
}

// event hooks

static PyObject * PyCtrlproxy_add_event_object(PyObject *self, PyObject *args, PyObject *keywds) {
    PyObject *result = NULL;
    PyObject *temp;
	struct callback_object *c;
    if (!PyArg_ParseTuple(args, "O:add_event_object",
								&temp))
		return NULL;



	if (!PyObject_TypeCheck(temp, &PyCtrlproxyEventType)) {
		PyErr_SetString(PyExc_ValueError, "Parameter is no Event Object");
		return NULL;
	}

	c = malloc(sizeof(struct callback_object));
	c->thread = PyThreadState_Get();
	c->callback = temp;
	Py_XINCREF(c->callback);         /* Add a reference to new callback */

	py_events = g_list_append(py_events, c);

	Py_INCREF(Py_None);
	return Py_None;
}


// networks interface
static PyObject * PyCtrlproxy_get_network(PyObject *self, PyObject *args, PyObject *kwds){
	static char *kwlist[] = {"network",NULL};
	GList *l = networks;
	char *net = NULL;
	PyObject *rv = NULL;

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "s:load_module", kwlist,
                                      &net))
		return rv;
	/* Find specified plugins' GModule and xmlNodePtr */
	printf("GET network %s\n",net);

	rv = PyList_New(0);
	while(l) {
		struct network *n = (struct network *)l->data;
		if(!strcasecmp(xmlGetProp(n->xmlConf, "name"), net)) {
			rv = (PyObject *)createNetworkObject(l->data);
			break;
		}
		l = g_list_next(l);
	}
	if(rv == NULL)
		PyErr_SetString(PyExc_ValueError, "Network not found");
	return rv;
}


static PyObject * PyCtrlproxy_connect_network(PyObject *self, PyObject *args, PyObject *kwds){
	static char *kwlist[] = {"network",NULL};
	char *net = NULL;
	char *nname;

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "s:connect_network", kwlist,
                                      &net))
		return NULL;
	/* Find specified plugins' GModule and xmlNodePtr */
	xmlNodePtr cur = xmlNode_networks->xmlChildrenNode;
	while(cur) {
		nname = xmlGetProp(cur, "name");
		if(nname && !strcmp(nname, net)) {
			xmlFree(nname);
			return createNetworkObject(connect_network(cur));
		}
	}

	PyErr_SetString(PyExc_ValueError, "Network not found");
	return NULL;
}


static PyObject * PyCtrlproxy_disconnect_network(PyObject *self, PyObject *args, PyObject *kwds){
	static char *kwlist[] = {"network",NULL};
	GList *l = networks;
	char *net = NULL;
	PyObject *rv = NULL;

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "s:load_module", kwlist,
                                      &net))
		return rv;
	/* Find specified plugins' GModule and xmlNodePtr */

	while(l) {
		struct network *n = (struct network *)l->data;
		if(!strcasecmp(xmlGetProp(n->xmlConf, "name"), net)) {
			close_network(n);
			Py_INCREF(Py_None);
			return Py_None;
		}
		l = g_list_next(l);
	}
	PyErr_SetString(PyExc_ValueError, "Network not found");
	return NULL;
}

// admin commands

static void in_py_admin_command(char **args, struct line *l)
{
	GList *g = admin_commands;
	PyObject *line = NULL;
	PyObject *arglist = NULL;

	while(g) {
		struct admin_callback_object *c = (struct admin_callback_object *)g->data;
		int i = 0;
		printf("%s",c->name);

		if(!strcasecmp(c->name, args[0])) {
			arglist = PyList_New(0);
			while(args[i] != NULL) {
				PyList_Append(arglist,PyString_FromString(args[i]));
				i++;
			}
			PyEval_RestoreThread(c->thread);
			line = (PyObject *)createLineObject(&l);
			//line = Py_None;
			//Py_INCREF(Py_None);
			PyObject_CallObject((PyObject *)c->callback, Py_BuildValue("(O,O)",arglist,line));
			PyErr_Print();
			break;
		}
		g = g_list_next(g);
	}
}


static PyObject * py_add_admin_command(PyObject *self, PyObject *args, PyObject *kwds) {
	static char *kwlist[] = {"name","callback",NULL};
	char *name = NULL;
	PyObject *temp = NULL;
	struct admin_callback_object *c;

	if(!plugin_loaded("admin") && !plugin_loaded("libadmin")) {
		PyErr_SetString(PyExc_TypeError, "Admin Module is required");
		return NULL;
	}

	if (! PyArg_ParseTupleAndKeywords(args, kwds, "sO:add_admin_command", kwlist,
                                      &name,&temp))
		return NULL;
	if (!PyCallable_Check(temp)) {
		PyErr_SetString(PyExc_TypeError, "Object must be callable");
		return NULL;
	}

	c = malloc(sizeof(struct admin_callback_object));
	c->thread = PyThreadState_Get();
	c->callback = temp;
	c->name = name;
	Py_INCREF(c->callback);         /* Add a reference to new callback */

	admin_commands = g_list_append(admin_commands, c);
	register_admin_command(name, in_py_admin_command);

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject * py_del_admin_command(PyObject *self, PyObject *args, PyObject *kwds) {
	static char *kwlist[] = {"name",NULL};
	char *name = NULL;
	GList *g = admin_commands;

	if (! PyArg_ParseTupleAndKeywords(args, kwds, "s:del_admin_command", kwlist,
                                      &name))
		return NULL;

	while(g) {
		struct admin_callback_object *c = (struct admin_callback_object *)g->data;

		if(!strcasecmp(c->name, name)) {
			unregister_admin_command(c->name);
			Py_XDECREF(c->callback);
			free(c->name);
			g_list_remove(admin_commands, c);
		}
		g = g_list_next(g);
	}
	free(name);
	Py_INCREF(Py_None);
	return Py_None;
}


// load und load plugins
static PyObject * PyCtrlproxy_loadmodule(PyObject *self, PyObject *args, PyObject *kwds){
	static char *kwlist[] = {"module",NULL};
	char *module = NULL;
	xmlNodePtr cur;

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "s:load_module", kwlist,
                                      &module))
		return NULL;

	cur = xmlNewNode(NULL, "plugin");
	xmlSetProp(cur, "file", module);
	xmlAddChild(xmlNode_plugins, cur);

	load_plugin(cur);

	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject * PyCtrlproxy_unloadmodule(PyObject *self, PyObject *args, PyObject *kwds){
	static char *kwlist[] = {"module",NULL};
	GList *g = plugins;
	char *module = NULL;

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "s:unload_module", kwlist,
                                      &module))
		return NULL;
	/* Find specified plugins' GModule and xmlNodePtr */
	while(g) {
		struct plugin *p = (struct plugin *)g->data;
		if(!strcmp(p->name, module)) {
			if(unload_plugin(p))
				plugins = g_list_remove(plugins, p);
        	Py_INCREF(Py_None);
			return Py_None;
		}
		g = g->next;
	}
	PyErr_Format(PyExc_TypeError, "module %s not loaded",module);
	free(module);
	return NULL;
}

static PyObject * PyCtrlproxy_listmodules(PyObject *self, PyObject *args, PyObject *kwds){
	GList *g = plugins;
	PyObject *rv = NULL;

	rv = PyList_New(0);
	while(g) {
		struct plugin *p = (struct plugin *)g->data;
		PyObject *pl = Py_BuildValue("s",p->name);
		PyList_Append(rv, pl);
		g = g->next;
	}
	return rv;
}

static PyObject * PyCtrlproxy_save_configuration(PyObject *self, PyObject *args, PyObject *kwds){
	save_configuration();
	return Py_None;
}

static PyObject * PyCtrlproxy_exit(PyObject *self, PyObject *args, PyObject *kwds){
	clean_exit();
	return Py_None;
}

static PyObject * PyCtrlproxy_getconfig(PyObject *self, PyObject *args, PyObject *kwds){
	static char *kwlist[] = {"type",NULL};
	char *type = NULL;
	PyObject *rv = NULL;

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "s:get_config", kwlist,
                                      &type))
		return NULL;

	if (!strcasecmp("networks",type))
		rv = PYCTRLPROXY_NODETOPYOB(xmlNode_networks)
	else if(!strcasecmp("plugins",type))
		rv = PYCTRLPROXY_NODETOPYOB(xmlNode_plugins)
	else
		rv = PyInstance_New(PyObject_GetAttrString(xml_module,"xmlDoc"),Py_BuildValue("(O)",libxml_xmlDocPtrWrap((xmlDocPtr) configuration)),NULL);

	return rv;
}

static PyObject * PyCtrlproxy_getpath(PyObject *self, PyObject *args, PyObject *kwds){
	static char *kwlist[] = {"type",NULL};
	char *part = NULL;
	PyObject *rv = NULL;

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "|s:get_path", kwlist,
                                      &part))
		return NULL;
	if(!strcasecmp("config", part)) {
		return PyString_FromString(ctrlproxy_path(""));
	}

	Py_INCREF(Py_None);
	return Py_None;
}


static PyMethodDef CtrlproxyMethods[] = {
	// hooks
    {"add_rcv_hook",  (PyCFunction)PyCtrlproxy_add_rcv_data_hook, METH_VARARGS | METH_KEYWORDS,
     "Add a data rcv hook"},
    {"del_rcv_hook",  (PyCFunction)PyCtrlproxy_del_rcv_data_hook, METH_VARARGS | METH_KEYWORDS,
     "Delete a data rcv hook"},
    {"add_new_client_hook",  (PyCFunction)PyCtrlproxy_add_new_client_hook, METH_VARARGS | METH_KEYWORDS,
     "Add a hook for new clients"},
    {"add_motd_hook",  (PyCFunction)PyCtrlproxy_add_motd_hook, METH_VARARGS | METH_KEYWORDS,
     "Add a motd hook"},
    {"add_server_connected_hook",  (PyCFunction)PyCtrlproxy_add_server_connected_hook, METH_VARARGS | METH_KEYWORDS,
     "Add server connected hook"},
    {"add_server_disconnected_hook",  (PyCFunction)PyCtrlproxy_add_server_disconnected_hook, METH_VARARGS | METH_KEYWORDS,
     "Server disconnected hook"},
//    {"add_del_client_hook",  (PyCFunction)py_del_new_client_hook, METH_VARARGS | METH_KEYWORDS,
//     "Add a hook for new clients"},
    {"add_lose_client_hook",  (PyCFunction)PyCtrlproxy_add_lose_client_hook, METH_VARARGS | METH_KEYWORDS,
     "Add a hook for lose clients"},
    {"add_event_object",  (PyCFunction)PyCtrlproxy_add_event_object, METH_VARARGS | METH_KEYWORDS,
     "Registers a event Object"},
	// networks
	{"list_networks",  (PyCFunction)PyCtrlproxy_list_networks, METH_VARARGS | METH_KEYWORDS,
     "Return a list of networks"},
    {"get_network",  (PyCFunction)PyCtrlproxy_get_network, METH_VARARGS | METH_KEYWORDS,
	 "Return a network object"},
    {"disconnect_network",  (PyCFunction)PyCtrlproxy_disconnect_network, METH_VARARGS | METH_KEYWORDS,
	 "Disconnect a Network"},
    {"connect_network",  (PyCFunction)PyCtrlproxy_connect_network, METH_VARARGS | METH_KEYWORDS,
	 "Connect a Network"},
	// admin commands
    {"add_admin_command",  (PyCFunction)py_add_admin_command, METH_VARARGS | METH_KEYWORDS,
     "Registers an admin command"},
    {"del_admin_command",  (PyCFunction)py_del_admin_command, METH_VARARGS | METH_KEYWORDS,
     "Unregisters an admin command"},
	 // module support
	{"load_module",  (PyCFunction)PyCtrlproxy_loadmodule, METH_VARARGS | METH_KEYWORDS,
     "Load a module"},
    {"unload_module",  (PyCFunction)PyCtrlproxy_unloadmodule, METH_VARARGS | METH_KEYWORDS,
     "Unload a module"},
    {"list_modules",  (PyCFunction)PyCtrlproxy_listmodules, METH_VARARGS,
	"List all loaded modules"},
	// config
    {"get_config",  (PyCFunction)PyCtrlproxy_getconfig, METH_VARARGS | METH_KEYWORDS,
	"Return xmlnode of config"},
    {"get_path",  (PyCFunction)PyCtrlproxy_getpath, METH_VARARGS | METH_KEYWORDS,
	"Get specific path"},
    {"save_config",  (PyCFunction)PyCtrlproxy_save_configuration, METH_VARARGS | METH_KEYWORDS,
	"Save the configuration"},
	// misc
    {"exit",  (PyCFunction)PyCtrlproxy_exit, METH_VARARGS | METH_KEYWORDS,
	"Exit ctrlproxy"},
	{NULL, NULL, 0, NULL}        /* Sentinel */
};

gboolean in_load(char *c,PyObject *args, struct line *l) {
	PyThreadState *ni;
	FILE *fp;
	struct script_thread *s;
	PyObject *m;
	PyObject *sys;
	PyCtrlproxyLog *pystdout;
	PyCtrlproxyLog *pystderr;
	PyThreadState *newInterpreter;
	int rv = TRUE;

	if (PyType_Ready(&PyCtrlproxyLogType) < 0)
        return FALSE;
	if (PyType_Ready(&PyCtrlproxyLineType) < 0)
        return FALSE;
	if (PyType_Ready(&PyCtrlproxyClientType) < 0)
        return FALSE;
	if (PyType_Ready(&PyCtrlproxyNetworkType) < 0)
        return FALSE;
	if (PyType_Ready(&PyCtrlproxyNickType) < 0)
        return FALSE;
	if (PyType_Ready(&PyCtrlproxyChannelType) < 0)
        return FALSE;
	if (PyType_Ready(&PyCtrlproxyEventType) < 0)
        return FALSE;

	fp = fopen(c,"r");
	if(fp == NULL)
		return FALSE;

	PyEval_AcquireLock();
	newInterpreter = Py_NewInterpreter();
	ni = PyThreadState_New(newInterpreter->interp);
	if(ni == NULL) {
		PyEval_ReleaseLock();
		return FALSE;
	}
	PyThreadState_Swap(ni);

	printf("CREATED THREAD %i\n",ni->thread_id);

	g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "Loading script[%i] %s", sid+1, c);

	xml_module = PyImport_ImportModule("libxml2");
	xml_modulemod = PyImport_ImportModule("libxml2mod");
	xml_node = PyObject_GetAttrString(xml_module,"xmlNode");

	PyImport_ImportModule("time");
	PyImport_AddModule("ctrlproxy");
	m = Py_InitModule("ctrlproxy", CtrlproxyMethods);

	if (m == NULL)
      return FALSE;

	PyModule_AddIntConstant(m, "RECURSION", G_LOG_FLAG_RECURSION);
	PyModule_AddIntConstant(m, "FATAL", G_LOG_FLAG_FATAL);
	PyModule_AddIntConstant(m, "ERROR", G_LOG_LEVEL_ERROR);
	PyModule_AddIntConstant(m, "CRITICAL", G_LOG_LEVEL_CRITICAL);
	PyModule_AddIntConstant(m, "WARNING", G_LOG_LEVEL_WARNING);
	PyModule_AddIntConstant(m, "MESSAGE", G_LOG_LEVEL_MESSAGE);
	PyModule_AddIntConstant(m, "INFO", G_LOG_LEVEL_INFO);
	PyModule_AddIntConstant(m, "DEBUG", G_LOG_LEVEL_DEBUG);
	PyModule_AddIntConstant(m, "LINE_IS_PRIVATE", LINE_IS_PRIVATE);
	PyModule_AddIntConstant(m, "LINE_DONT_SEND", LINE_DONT_SEND);
	PyModule_AddIntConstant(m, "LINE_STOP_PROCESSING", LINE_STOP_PROCESSING);
	PyModule_AddIntConstant(m, "LINE_NO_LOGGING", LINE_NO_LOGGING);

	// data_direction
	PyModule_AddIntConstant(m, "UNKNOWN", UNKNOWN);
	PyModule_AddIntConstant(m, "TO_SERVER", TO_SERVER);
	PyModule_AddIntConstant(m, "FROM_SERVER", FROM_SERVER);
	PyModule_AddIntConstant(m, "TO_CLIENT", FROM_SERVER);

	Py_INCREF(&PyCtrlproxyLogType);
	Py_INCREF(&PyCtrlproxyLineType);
	Py_INCREF(&PyCtrlproxyNetworkType);
	Py_INCREF(&PyCtrlproxyClientType);
	Py_INCREF(&PyCtrlproxyNickType);
	Py_INCREF(&PyCtrlproxyChannelType);
	Py_INCREF(&PyCtrlproxyNetworkType);
	Py_INCREF(&PyCtrlproxyEventType);

    PyModule_AddObject(m, "Log", (PyObject *)&PyCtrlproxyLogType);
    PyModule_AddObject(m, "Line", (PyObject *)&PyCtrlproxyLineType);
    PyModule_AddObject(m, "Client", (PyObject *)&PyCtrlproxyClientType);
	PyModule_AddObject(m, "Network", (PyObject *)&PyCtrlproxyNetworkType);
	PyModule_AddObject(m, "args", args);
	PyModule_AddObject(m, "Nick", (PyObject *)&PyCtrlproxyNickType);
	PyModule_AddObject(m, "Channel", (PyObject *)&PyCtrlproxyChannelType);
	PyModule_AddObject(m, "Event", (PyObject *)&PyCtrlproxyEventType);

	// we cache this objects staticly

	sys = PyImport_AddModule("sys");

	// set default stdout and stderr
	if(py_redirect_stdout) {
		pystdout = PyObject_New(PyCtrlproxyLog, &PyCtrlproxyLogType);
		if(pystdout != NULL) {
			PyObject_CallMethod((PyObject *)pystdout, "__init__", "i", G_LOG_LEVEL_INFO);
			PyObject_SetAttrString(sys, "stdout", (PyObject *)pystdout);
		}
	}
	if(py_redirect_stderr) {
		pystderr = PyObject_New(PyCtrlproxyLog, &PyCtrlproxyLogType);
		if(pystderr != NULL) {
			PyObject_CallMethod((PyObject *)pystderr, "__init__", "i", G_LOG_LEVEL_WARNING);
			PyObject_SetAttrString(sys, "stderr", (PyObject *)pystderr);
		}
	}
	// extend module path and add tools object
	PyList_Append(PyObject_GetAttrString(sys,"path"),PyString_FromString(pylibdir));
	PyObject_Print(PyObject_GetAttrString(sys,"path"), stdout, 0);

	PyImport_ImportModule("ctrlproxy_tools");
	PyModule_AddObject(m, "tools", PyImport_AddModule("ctrlproxy_tools"));

	s = malloc(sizeof(struct script_thread));
	s->thread = ni;
	s->id = GINT_TO_POINTER(sid++);
	s->script = strdup(c);
	loaded_scripts = g_list_append(loaded_scripts, s);

	PyRun_SimpleFile(fp, c);

	PyThreadState_Swap(NULL);
	PyEval_ReleaseLock();

	if(l != NULL && (!plugin_loaded("libnoticelog") || !plugin_loaded("noticelog")))
		admin_out(l,"Load of script #%i complete",sid);
	g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO,"Load of script #%i complete",sid);

	return rv;
}

#define PYCTRLPROXY_REMOVECALLBACKFROMTHREAD(LIST) \
	gu = LIST; \
	while(gu) { \
		struct callback_object *c = (struct callback_object *)gu->data; \
		if(c->thread == p->thread) { \
			printf("REMOVE CALLBACK: %p\n", c->thread); \
			LIST = g_list_delete_link(LIST, gu); \
			gu = g_list_first(LIST); \
		} else \
			gu = gu->next; \
	}

gboolean in_unload(int i,struct line *l) {
	GList *g = loaded_scripts;
	GList *gu = NULL;
	while(g) {
		struct script_thread *p = (struct script_thread *)g->data;
		if((int)p->id == i) {
			PyEval_AcquireLock();

			// remove callbacks
			PYCTRLPROXY_REMOVECALLBACKFROMTHREAD(py_rcv_hooks)
			PYCTRLPROXY_REMOVECALLBACKFROMTHREAD(py_new_client_hooks)
			PYCTRLPROXY_REMOVECALLBACKFROMTHREAD(py_lose_client_hooks)
			PYCTRLPROXY_REMOVECALLBACKFROMTHREAD(py_motd_hooks)
			PYCTRLPROXY_REMOVECALLBACKFROMTHREAD(py_server_connected_hooks)
			PYCTRLPROXY_REMOVECALLBACKFROMTHREAD(py_server_disconnected_hooks)
			PYCTRLPROXY_REMOVECALLBACKFROMTHREAD(py_events)

			// remove admin commands
			gu = admin_commands;
			while(gu) {
				struct admin_callback_object *c = (struct admin_callback_object *)gu->data;
				if(c->thread == p->thread) {
					printf("REMOVE ADMIN COMMAND: %s\n",c->name);
					unregister_admin_command(c->name);
					admin_commands = g_list_remove(admin_commands,gu);
				}
				gu = gu->next;
			}
			PyThreadState_Swap(NULL);
			//PyThreadState_Clear(p->thread);
			//
			//Py_EndInterpreter(p->thread);
			//PyEval_ReleaseThread(p->thread);
			PyInterpreterState_Clear(p->thread->interp);
			PyInterpreterState_Delete(p->thread->interp);
			//PyThreadState_Delete(p->thread);
			loaded_scripts = g_list_remove(loaded_scripts,p);
			PyEval_ReleaseLock();
			g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "Unload script %i complete", i);
			if(l != NULL && (!plugin_loaded("libnoticelog") || !plugin_loaded("noticelog")))
				admin_out(l, "Unload script #%i complete",i);
			return TRUE;
		}
		g = g->next;
	}
	if(l != NULL)
		admin_out(l, "Couldn't unload script #%i",i);
	return FALSE;
}

/* Load script configured in the config.
When name is NULL, every script with autoload = 1 will be loaded.
*/
static void in_load_from_config(char *name) {
	xmlNodePtr cur = xmlConf->xmlChildrenNode;
	while(cur) {
		xmlNodePtr curargs;
		PyObject *args = NULL;
		char *file = NULL;
		char *enabled = 0;
		if(!xmlIsBlankNode(cur) && !strcmp(cur->name, "script")) {
				enabled = xmlGetProp(cur, "autoload");
				if((enabled == NULL || atoi(enabled) == 0) && name == NULL) {
					cur = cur->next;
					continue;
				}
				// build arguments list
				args = PyDict_New();
				curargs = cur->xmlChildrenNode;
				while(curargs) {
					if(!xmlIsBlankNode(cur) && !strcmp(curargs->name, "argument") && xmlHasProp(curargs, "name"))
						PyDict_SetItemString(args,xmlGetProp(curargs, "name"),PyString_FromString(xmlNodeGetContent(curargs)));
					if(!xmlIsBlankNode(cur) && !strcmp(curargs->name, "file"))
						file = xmlNodeGetContent(curargs);
					curargs = curargs->next;
				}

				if(name != NULL && strcasecmp(name, file)) {
					cur = cur->next;
					continue;
				}
				in_load(file, args, NULL);
		}
		cur = cur->next;
	}
}

static void in_admin_command(char **args, struct line *l)
{
	if(args[1] != NULL && !strcasecmp("load", args[1]) && args[2] != NULL) {
		PyObject *pargs = PyDict_New();
		int i = 3;
		printf("Python Load %s\n", args[2]);
		for(;args[i] != NULL;i++) {
			char **na = g_strsplit(args[i],"=",2);
			if(na[0] != NULL) {
				if(na[1] != NULL)
					PyDict_SetItemString(pargs, na[0], PyString_FromString(na[1]));
				else {
					Py_INCREF(Py_None);
					PyDict_SetItemString(pargs, na[0], Py_None);
				}
			}
		}
		if(!access(args[2], F_OK))
			in_load(args[2],pargs, l);
		else
			admin_out(l, "Error, File does not exist");
		return;
	}
	// load script configured in the config file
	if(args[1] != NULL && !strcasecmp("loadc", args[1]) && args[2] != NULL) {
		in_load_from_config(args[2]);
	}

	if(args[1] != NULL && !strcasecmp("unload", args[1]) && args[2] != NULL) {
		printf("Python Unload %i\n", atoi(args[2]));
		in_unload(atoi(args[2]),l);
		return;
	}
	if(args[1] != NULL && !strcasecmp("list", args[1])) {
		list_scripts(l);
		return;
	}
	if(args[1] != NULL && !strcasecmp("help", args[1])) {
		if(args[2] != NULL) {
			admin_out(l, "Help for %s",args[2]);
			if(!strcasecmp("LOAD", args[2])) {
				admin_out(l, "Load a python script. Arguments are handeled like bash");
			} else if(!strcasecmp("LOADC", args[2])) {
				admin_out(l, "Load a python script that is already configured in the configuration but not loaded");
			} else if(!strcasecmp("UNLOAD", args[2])) {
				admin_out(l, "Unload a the plugin with the given id");
			} else if(!strcasecmp("LIST", args[2])) {
				admin_out(l, "List all loaded scripts");
			} else if(!strcasecmp("HELP", args[2])) {
				admin_out(l, "Prints detailed informations about a command");
			} else {
				admin_out(l, "Unknown command");
			}
		} else {
			admin_out(l, "Command required");
		}
		return;
	}
	admin_out(l, "Need command");
	admin_out(l, "LOAD <filename> [arguments]");
	admin_out(l, "LOADC [filename]");
	admin_out(l, "UNLOAD #id");
	admin_out(l, "LIST");
	admin_out(l, "HELP [command]");
	//admin_out(l, "OPENSHELL");
	admin_out(l, "");
}

// load python scripts

static void init_finish() {
	g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "Loading Python scripts");
	in_load_from_config(NULL);
	list_scripts(NULL);
}


/*

gboolean thread_loop(gpointer user_data) {
	//GList *l = loaded_scripts;
	//while(l) {
		int rcv;
		GList *gl = loaded_scripts;
		printf("--------- Run Thread loop ---------\n");
		//struct script_thread *s = (struct script_thread *)l->data;
		PyThreadState_Swap(mainThreadState);
		while(gl) {
			struct script_thread *p = (struct script_thread *)gl->data;
			PyThreadState_Swap(p->thread);
			gl = gl->next;
		}
		//PyInterpreterState_Clear(mainThreadState->interp);
		//PyThreadState_Swap(mainInterpreterState);
		//PyEval_RestoreThread(mainThreadState);
		//
		if(PyErr_CheckSignals())
			printf("Got signal");
		//PyErr_Print();
		//PyThreadState_Clear(s->thread);
		PyThreadState_Swap(NULL);
		//PyEval_ReleaseThread(mainThreadState);
		PyEval_ReleaseLock();
		//l = g_list_next(l);
	//}

	PyInterpreterState *is = PyInterpreterState_Head();
	PyEval_AcquireLock();

	while(is) {
		PyThreadState *ts =  PyInterpreterState_ThreadHead(is);
		printf("IS: %p %i\n",is);
		while(ts) {
			PyThreadState_Swap(ts);
			if(PyRun_SimpleString("import os\nprint os.getpid()") == -1) {
				PyThreadState *ts2 = PyThreadState_Next(ts);
				printf("Deleted thread ? %i\n",ts->thread_id);
				PyThreadState_Swap(NULL);
				PyThreadState_Clear(ts);
				PyThreadState_Delete(ts);
				ts = ts2;
			} else {
				PyThreadState_Swap(NULL);
				printf("TS: %p %i\n",ts,ts->thread_id);
				ts = PyThreadState_Next(ts);
			}
		}
		is = PyInterpreterState_Next(is);

	}
	PyThreadState_Swap(NULL);
	PyEval_ReleaseLock();
	return TRUE;
}
*/


gboolean init_plugin(struct plugin *p) {
	PyObject *pyxml = NULL;
	xmlNodePtr cur;

	xmlConf = p->xmlConf;


	if(!plugin_loaded("admin") && !plugin_loaded("libadmin")) {
		g_warning("admin module required for dynamic load/unload of python scripts");
	} else {
		register_admin_command("PYTHON", in_admin_command);
	}

	//add_motd_hook("python_motd", in_motd_hook);
	cur = xmlConf->xmlChildrenNode;
	while(cur) {
		char *name;
		char *value;
		if(!xmlIsBlankNode(cur) && !strcmp(cur->name, "option")) {
			name = xmlGetProp(cur, "name");
			value = xmlNodeGetContent(cur);
			if (!strcasecmp(name, "redirect-stdout"))
				py_redirect_stdout = atoi(value);
			if (!strcasecmp(name, "redirect-stderr"))
				py_redirect_stderr = atoi(value);
		}
		cur = cur->next;
	}

	// init python

	Py_Initialize();
	PyEval_InitThreads();
	mainThreadState = PyThreadState_Get();

	pyxml = PyImport_ImportModule("libxml2mod");
	if(pyxml == NULL) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, "libxml2 python bindings are required");
		return FALSE;

	}


	/* Execute some Python statements (in module __main__) */
	//PyRun_SimpleString("import sys\n");
	//PyRun_SimpleString("print sys.builtin_module_names\n");
	//PyRun_SimpleString("print ctrlproxy.__dict__\n");
	//PyRun_SimpleString("print sys.modules.keys()\n");

	//cur = xmlFindChildByElementName(p->xmlConf, "load");
	PyEval_ReleaseLock();

	// load scripts
	add_filter("python", in_rcv_data);
	add_new_client_hook("python_new_client", in_new_client_hook);
	add_lose_client_hook("python_lose_client", in_lose_client_hook);
	add_motd_hook("python_motd", in_motd_hook);
	add_server_connected_hook("python_server_connected", in_server_connected_hook);
	add_server_disconnected_hook("python_server_disconnected", in_server_disconnected_hook);
	add_initialized_hook(&init_finish);

	return TRUE;
}

gboolean fini_plugin(struct plugin *p) {
	del_filter(in_rcv_data);
	del_new_client_hook("python_new_client");
	del_lose_client_hook("python_lose_client");
	del_lose_client_hook("python_motd");

	//g_hash_table_destroy(highlight_backlog); highlight_backlog = NULL;
	return TRUE;
}