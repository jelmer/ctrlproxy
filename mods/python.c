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
#include "python_libxml_wrap.h"

xmlNodePtr xmlConf;

// should go to ctrlproxy.h
static int TO_CLIENT = 3;

static GList *py_rcv_hooks = NULL;
static GList *py_lose_client_hooks = NULL;
static GList *py_new_client_hooks = NULL;
static GList *py_modt_hooks = NULL;
static GList *py_server_connected_hooks  = NULL;
static GList *py_server_disconnected_hooks = NULL;

static GList *loaded_scripts = NULL;
static GList *script_attributes = NULL;

static GList *admin_commands = NULL;

static PyObject *xml_module = NULL;
static PyObject *xml_modulemod = NULL;
static PyObject *xml_api_object = NULL;
static PyObject *xml_node = NULL;
//staticforward PyTypeObject PyCtrlproxyLog_Type;

static int sid = 0;

#define PYCTRLPROXY_NODETOPYOB(node) \
PyInstance_New(xml_node,Py_BuildValue("(O)",libxml_xmlNodePtrWrap((xmlNodePtr) node)),NULL);

#define PYCTRLPROXY_PYADDCALLBACK(function,callarray) \
 static PyObject * function(PyObject *self, PyObject *args, PyObject *keywds) { \
    PyObject *result = NULL; \
    PyObject *temp; \
	struct callback_object *c; \
	printf("ADDCALLBACK callarray"); \
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
	while(g) { \
		struct callback_object *c = (struct callback_object *)g->data; \
		PyObject *lo = CREATE(l); \
		PyObject *arglist = Py_BuildValue("(O)", lo); \
		PyEval_RestoreThread(c->thread); \
    	r = PyObject_CallObject((PyObject *)c->callback, arglist); \
		PyErr_Print(); \
		g = g->next; \
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
	printf("CREATE OBJ %p\n",c); \
	rv->ptr = c; \
	return (PyObject *)rv; \
}

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
} PyCtrlproxyLog;


typedef struct {
	PyObject_HEAD
	//PyObject *ptr;
	void *ptr;
} PyCtrlproxyObject;

static void PyCtrlproxyObject_dealloc(PyCtrlproxyObject *self)
{
	printf("DEALLOC ptr %p\n",self->ptr);
    self->ob_type->tp_free((PyObject *)self);
};

// wrapper object for stdout, stderr -> g_log

static void PyCtrlproxyLog_dealloc(PyCtrlproxyLog *self)
{
	free((int *)self->log_level);
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

	smsg = g_strsplit (msg,"\n",0);
	for(i = 0;smsg[i] != NULL;i++) {
		if(smsg[i] != NULL && strcasecmp(smsg[i],"")) {
			g_log(G_LOG_DOMAIN, (int)log_level, "%s", smsg[i]);
			//printf("%s\n", smsg[i]);
		}
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

// NETWORK

static PyObject * PyCtrlproxyNetwork_get(PyCtrlproxyObject *self, void *closure) {
	printf("--GET%s\n", (char *)closure);
	return PyString_FromString("Blubb");
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

	struct network *n = (struct network *)self->ptr;

	if (! PyArg_ParseTupleAndKeywords(args, kwds, "s|d:send", kwlist,
                                      &msg,&dir))
		return NULL;

	if (dir == TO_CLIENT) {
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

static PyMethodDef PyCtrlproxyNetwork_methods[] = {
    {"send", (PyCFunction)PyCtrlproxyNetwork_send,METH_VARARGS | METH_KEYWORDS, "Write message into the network"},
	{NULL}                      /* Sentinel */
};


static PyGetSetDef PyCtrlproxyNetwork_GetSetDef[] = {
    {"xmlConf",(getter)PyCtrlproxyNetwork_get,(setter)PyCtrlproxyNetwork_set,
	"XML Node of the network","xmlConf"},
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
    {"outgoing",(getter)PyCtrlproxyNetwork_get,(setter)PyCtrlproxyNetwork_set,
	"Outgoing Transport Object","outgoing"},
    {"incoming",(getter)PyCtrlproxyNetwork_get,(setter)PyCtrlproxyNetwork_set,
	"Incoming Transport Object","incoming"},
	{NULL}
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
	struct client *l = NULL;
	PyObject *rv = NULL;

	printf("Client access at %p\n",self->ptr);

	if(self->ptr == NULL) {
		PyErr_SetString(PyExc_TypeError, "Line Object is not initialized");
		return Py_None;
	}
	l = (struct client *)self->ptr;
	if(l == NULL) {
		PyErr_SetString(PyExc_TypeError, "Object seems to be deleted");
		return Py_None;
	}
	printf("Client NET: %p\n",&l->network);
	printf("Client NETXML: %p\n",&l->network->xmlConf);

	if(!strcmp(closure,"network")) {
		rv = (PyObject *)createNetworkObject(&l->network);
	} else if(!strcmp(closure,"transport_context")) {
		//rv = PyString_FromString(l->authenticated);
	} else if(!strcmp(closure,"connect_time")) {
		rv = PyInt_FromLong((long)l->connect_time);
	} else if(!strcmp(closure,"did_nick_change")) {
		rv = PyString_FromString(&l->did_nick_change);
	} else if(!strcmp(closure,"nick")) {
		rv = PyString_FromString(xmlGetProp(l->network->xmlConf, "nick"));
	}
	return rv;
}

static PyObject * PyCtrlproxyClient_send(PyCtrlproxyObject *self, PyObject *args, PyObject *kwds) {
	static char *kwlist[] = {"msg",NULL};
	char *msg;
	struct client *c = NULL;

	printf("Client send at %p\n",self->ptr);

	if(self->ptr == NULL) {
		PyErr_SetString(PyExc_TypeError, "Line Object is not initialized");
		return NULL;
	}
	c = (struct client *)self->ptr;
	if(c == NULL) {
		PyErr_SetString(PyExc_TypeError, "Object seems to be deleted");
		return NULL;
	}
	//l = (struct client *)PyCObject_AsVoidPtr(self->ptr);

	if (! PyArg_ParseTupleAndKeywords(args, kwds, "s:send", kwlist,
                                      &msg))
		return NULL;

	printf("Net: %p\n", c->network);

	if(c->incoming == NULL) {
		PyErr_SetString(PyExc_TypeError, "Not writeable");
		return NULL;
	}
	printf("li: %p\n",&c->incoming);
	transport_write(c->incoming, msg);

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject * PyCtrlproxyClient_sendNotice(PyCtrlproxyObject *self, PyObject *args, PyObject *kwds) {
	static char *kwlist[] = {"msg",NULL};
	char *msg, *nick, *tot, *server_name;
	PyObject *rv = NULL;
	struct client *l = NULL;

	if(self->ptr == NULL) {
		PyErr_SetString(PyExc_TypeError, "Line Object is not initialized");
		return NULL;
	}
	l = (struct client *)self->ptr;
	if(l == NULL) {
		PyErr_SetString(PyExc_TypeError, "Object seems to be deleted");
		return NULL;
	}

	if (! PyArg_ParseTupleAndKeywords(args, kwds, "s:sendNotice", kwlist,
                                      &msg))
		return NULL;

	if(l->network == NULL) {
		PyErr_SetString(PyExc_TypeError, "No Network available");
		return NULL;
	}
	printf("XM: %p\n",l->network->xmlConf);
	//xmlDebugDumpNode(stdout,l->network->xmlConf,0);
	nick = xmlGetProp(l->network->xmlConf, "nick");
	server_name = xmlGetProp(l->network->xmlConf, "name");

	asprintf(&tot, ":ctrlproxy!ctrlproxy@%s NOTICE %s :%s\r\n", server_name, nick, msg);
	rv = PyCtrlproxyClient_send(self, Py_BuildValue("(s)",tot),NULL);

	xmlFree(nick);
	xmlFree(server_name);
	return rv;
}

static PyMethodDef PyCtrlproxyClient_methods[] = {
    {"send", (PyCFunction)PyCtrlproxyClient_send,METH_VARARGS | METH_KEYWORDS, "Sends a raw string to the client"},
    {"send_notice", (PyCFunction)PyCtrlproxyClient_sendNotice,METH_VARARGS | METH_KEYWORDS, ""},
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

	if(self->ptr == NULL) {
		PyErr_SetString(PyExc_TypeError, "Line Object is not initialized");
		return NULL;
	}
	l = (struct line *)self->ptr;
	if(l == NULL) {
		PyErr_SetString(PyExc_TypeError, "Object seems to be deleted");
		return NULL;
	}
	if(!strcmp(closure,"direction")) {
		rv = PyInt_FromLong((long)l->direction);
	} else if(!strcmp(closure,"options")) {
		rv = PyInt_FromLong((long)l->options);
	} else if(!strcmp(closure,"network")) {
		rv = (PyObject *)createNetworkObject(&l->network);
	} else if(!strcmp(closure,"client")) {
		/*if(FROM_SERVER == l->direction) {
			PyErr_SetString(PyExc_TypeError, "Line comes from Server and has no Client");
			return NULL;
		} else {
		*/
			rv = (PyObject *)createClientObject(&l->client);
		//}
	} else if(!strcmp(closure,"origin")) {
		rv = PyString_FromString(l->origin);
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
		PyErr_SetString(PyExc_TypeError, "Line Object is not initialized");
		return -1;
	}
	l = (struct line *)self->ptr;
	if(l == NULL) {
		PyErr_SetString(PyExc_TypeError, "Object seems to be deleted");
		return -1;
	}

	if(!strcmp(closure,"options")) {
		if(!PyInt_Check(var)) {
			PyErr_SetString(PyExc_ValueError, "Var is no int");
			return -1;
		}
		l->options = (int)PyInt_AsLong(var);
	} else {
		PyErr_SetString(PyExc_TypeError, "Option is readonly");
	}
	Py_INCREF(Py_None);

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
			admin_out(l, "#%d: %s",p->id,p->script);
		printf("ID:%d:%s\n",p->id,p->script);
		g = g->next;
	}
}


static void in_admin_command(char **args, struct line *l)
{
	if(args[1] != NULL && !strcasecmp("load", args[1]) && args[2] != NULL) {
		printf("Python Load %s\n", args[2]);
		in_load(args[2],NULL);
		return;
	}
	if(args[1] != NULL && !strcasecmp("unload", args[1]) && args[2] != NULL) {
		printf("Python Unload %s\n", args[2]);
		in_unload(atoi(args[2]),l);
		return;
	}
	if(args[1] != NULL && !strcasecmp("list", args[1])) {
		list_scripts(l);
		return;
	}

	admin_out(l, "Need command");
	admin_out(l, "LOAD [filename]");
	admin_out(l, "UNLOAD #id");
	admin_out(l, "LIST");
	admin_out(l, "OPENSHELL");
	admin_out(l, "");
}


// recv hooks

PYCTRLPROXY_PYADDCALLBACK(py_add_rcv_data_hook,py_rcv_hooks)


static gboolean in_rcv_data(struct line *l) {
	// python shell support
	printf("ORGCLIENT %p\n",&l->client);
	printf("ORGNET %p\n",&l->network);
	printf("ORGCLIENTNET %p\n",&l->client->network);
	printf("ORGXML %p\n",&l->network->xmlConf);
	//xmlDebugDumpNode(stdout,l->network->xmlConf,0);

	PYCTRLPROXY_PYCALLPYCALLBACKS(py_rcv_hooks,createLineObject)

/*	if(l->direction == TO_SERVER && !strcasecmp(l->args[0], "PRIVMSG") &&
	   	strcasecmp("!PYSHELL",l->args[1]) == 0) {
		l->options |= LINE_DONT_SEND | LINE_IS_PRIVATE;
		printf("PYSHELLSUPPORT\n");
	}
*/
	return TRUE;
}

static PyObject * py_del_rcv_data_hook(PyObject *self, PyObject *args, PyObject *keywds) {
    char *command;

    if (!PyArg_ParseTuple(args, "s", &command))
        return NULL;
	Py_INCREF(Py_None);
    return Py_None;
}


// new client hooks

PYCTRLPROXY_PYADDCALLBACK(py_add_new_client_hook,py_new_client_hooks)

static gboolean in_new_client_hook(struct client *l) {
	PYCTRLPROXY_PYCALLPYCALLBACKS(py_new_client_hooks,createClientObject)
	return 1;
}




// lose client hooks

PYCTRLPROXY_PYADDCALLBACK(py_add_lose_client_hook,py_lose_client_hooks)

static void in_lose_client_hook(struct client *l)
{
	PYCTRLPROXY_PYCALLPYCALLBACKS(py_lose_client_hooks,createClientObject)
}


// motd hooks

PYCTRLPROXY_PYADDCALLBACK(py_modt_hook,py_modt_hooks)

char ** in_motd_hook(struct network *n)
{
	char **rv = NULL;
	GList *g = py_modt_hooks;
	PyObject *r = NULL;
	int i = 0;
	int nrlines = 0;

	while(g) {
		struct callback_object *c = (struct callback_object *)g->data;
		PyObject *lo = createNetworkObject(n);
		PyObject *arglist = Py_BuildValue("(O)", lo);
		PyEval_RestoreThread(c->thread);
    	r = PyObject_CallObject((PyObject *)c->callback, arglist);
		if(!PyList_Check(r)) {
			PyErr_SetString(PyExc_ValueError, "Return Value wrong");
			PyErr_Print();
		} else {
			for(i = 0; i < PyList_Size(r); i++) {
				rv[nrlines] = PyString_AsString(PyObject_Str(PyList_GetItem(r,i)));
				nrlines++;
				rv[nrlines] = NULL;
			}
			rv[nrlines] = "\r\n";
			nrlines++;
			rv[nrlines] = NULL;
		}

		g = g->next;
	}

	return rv;
}


// server connect hook

PYCTRLPROXY_PYADDCALLBACK(py_add_server_connected_hook,py_server_connected_hooks)

static void in_server_connected_hook(struct network *l) {
	PYCTRLPROXY_PYCALLPYCALLBACKS(py_server_connected_hooks,createNetworkObject)
}

// server connect hook

PYCTRLPROXY_PYADDCALLBACK(py_add_server_disconnected_hook,py_server_disconnected_hooks)

static void in_server_disconnected_hook(struct network *l) {
	PYCTRLPROXY_PYCALLPYCALLBACKS(py_server_disconnected_hooks,createNetworkObject)
}


static PyObject * PyCtrlproxy_listnetworks(PyObject *self, PyObject *args, PyObject *kwds){
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

// networks interface
static PyObject * PyCtrlproxy_getnetwork(PyObject *self, PyObject *args, PyObject *kwds){
	static char *kwlist[] = {"network",NULL};
	GList *l = networks;
	char *net = NULL;
	PyObject *rv = NULL;

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "s:load_module", kwlist,
                                      &net))
		return rv;
	/* Find specified plugins' GModule and xmlNodePtr */
	rv = PyList_New(0);
	while(l) {
		struct network *n = (struct network *)l->data;
		if(!strcasecmp(xmlGetProp(n->xmlConf, "name"), net)) {
			//rv = (PyObject *)createNetworkObject(&n);
			break;
		}
		l = g_list_next(l);
	}

	return rv;
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

    //if (! PyArg_ParseTuple(args, kwds, ":list_modules"))
	//	return NULL;
	/* Find specified plugins' GModule and xmlNodePtr */
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

/*static PyObject * PyCtrlproxy_get_configuration(PyObject *self, PyObject *args, PyObject *kwds){
	return PyString_FromString(configuration_file);
}
*/
static PyObject * PyCtrlproxy_exit(PyObject *self, PyObject *args, PyObject *kwds){
	clean_exit();
	return Py_None;
}

static PyObject * PyCtrlproxy_read_config(PyObject *self, PyObject *args, PyObject *kwds){
	char *config = NULL;
	static char *kwlist[] = {"config",NULL};
    if (! PyArg_ParseTupleAndKeywords(args, kwds, "s:read_config", kwlist,
                                      &config))

	readConfig(config);
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

	//Py_XINCREF(rv);
	return rv;
}


static PyMethodDef CtrlproxyMethods[] = {
	// hooks
    {"add_rcv_hook",  (PyCFunction)py_add_rcv_data_hook, METH_VARARGS | METH_KEYWORDS,
     "Add a data rcv hook"},
    {"del_rcv_hook",  (PyCFunction)py_del_rcv_data_hook, METH_VARARGS | METH_KEYWORDS,
     "Delete a data rcv hook"},
    {"add_new_client_hook",  (PyCFunction)py_add_new_client_hook, METH_VARARGS | METH_KEYWORDS,
     "Add a hook for new clients"},
//    {"add_del_client_hook",  (PyCFunction)py_del_new_client_hook, METH_VARARGS | METH_KEYWORDS,
//     "Add a hook for new clients"},
    {"add_lose_client_hook",  (PyCFunction)py_add_lose_client_hook, METH_VARARGS | METH_KEYWORDS,
     "Add a hook for lose clients"},
	// networks
    {"list_networks",  (PyCFunction)PyCtrlproxy_listnetworks, METH_VARARGS | METH_KEYWORDS,
     "Return a list of networks"},
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
    {"save_config",  (PyCFunction)PyCtrlproxy_save_configuration, METH_VARARGS | METH_KEYWORDS,
	"Save the configuration"},
    {"read_config",  (PyCFunction)PyCtrlproxy_read_config, METH_VARARGS | METH_KEYWORDS,
	"Read configuration file"},
	// misc
    {"exit",  (PyCFunction)PyCtrlproxy_exit, METH_VARARGS | METH_KEYWORDS,
	"Exit ctrlproxy"},
    //{"get_config_file",  (PyCFunction)PyCtrlproxy_get_configuration, METH_VARARGS | METH_KEYWORDS,
	//"Return configuration filename"},
	{NULL, NULL, 0, NULL}        /* Sentinel */
};

// load python script
gboolean in_load(char *c,PyObject *attrs) {
	PyThreadState *ni;
	FILE *fp;
	struct script_thread *s;
	PyObject *m;
	PyObject *sys;
	PyCtrlproxyLog *stdout;
	PyCtrlproxyLog *stderr;

	if (PyType_Ready(&PyCtrlproxyLogType) < 0) return FALSE;
	if (PyType_Ready(&PyCtrlproxyLineType) < 0) return FALSE;
	if (PyType_Ready(&PyCtrlproxyClientType) < 0) return FALSE;
	if (PyType_Ready(&PyCtrlproxyNetworkType) < 0) return FALSE;
	/*if (PyType_Ready(&PyCtrlproxyNickType) < 0) return FALSE;
	if (PyType_Ready(&PyCtrlproxyChannelType) < 0) return FALSE;
	*/

	fp = fopen(c,"r");
	if(fp == NULL)
		return FALSE;
	ni = Py_NewInterpreter();
	if(ni == NULL)
		return FALSE;
	g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "Loading script[%i] %s", sid+1, c);

	// replace sys.stdout and sys.stderr
	xml_module = PyImport_ImportModule("libxml2");
	xml_modulemod = PyImport_ImportModule("libxml2mod");
	xml_node = PyObject_GetAttrString(xml_module,"xmlNode");

	PyImport_ImportModule("time");
	PyImport_AddModule("ctrlproxy");
	m = Py_InitModule("ctrlproxy", CtrlproxyMethods);

	if (m == NULL) return FALSE;

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
	PyModule_AddIntConstant(m, "TO_CLIENT", TO_CLIENT);

	Py_INCREF(&PyCtrlproxyLogType);
	Py_INCREF(&PyCtrlproxyLineType);
	Py_INCREF(&PyCtrlproxyNetworkType);
	Py_INCREF(&PyCtrlproxyClientType);
	//Py_INCREF(&PyCtrlproxyNickType);
	//Py_INCREF(&PyCtrlproxyChannelType);
	Py_INCREF(&PyCtrlproxyNetworkType);
    PyModule_AddObject(m, "Log", (PyObject *)&PyCtrlproxyLogType);
    PyModule_AddObject(m, "Line", (PyObject *)&PyCtrlproxyLineType);
    PyModule_AddObject(m, "Client", (PyObject *)&PyCtrlproxyClientType);
	PyModule_AddObject(m, "Network", (PyObject *)&PyCtrlproxyNetworkType);
	//PyModule_AddObject(m, "Nick", (PyObject *)&PyCtrlproxyNickType);
	//PyModule_AddObject(m, "Channel", (PyObject *)&PyCtrlproxyChannelType);

	// we cache this objects staticly

	sys = PyImport_AddModule("sys");

	// set default stdout and stderr
	stdout = PyObject_New(PyCtrlproxyLog, &PyCtrlproxyLogType);
	Py_INCREF(stdout);
	if(stdout != NULL) {
		PyObject_CallMethod((PyObject *)stdout, "__init__", "i", G_LOG_LEVEL_INFO);
		PyObject_SetAttrString(sys, "stdout", (PyObject *)stdout);
	}

	stderr = PyObject_New(PyCtrlproxyLog, &PyCtrlproxyLogType);
	Py_INCREF(stderr);
	if(stderr != NULL) {
		PyObject_CallMethod((PyObject *)stderr, "__init__", "i", G_LOG_LEVEL_WARNING);
		PyObject_SetAttrString(sys, "sterr", (PyObject *)stderr);
	}

	s = malloc(sizeof(struct script_thread));
	s->thread = ni;
	s->id = GINT_TO_POINTER(sid++);
	s->script = strdup(c);
	loaded_scripts = g_list_append(loaded_scripts, s);

	PyRun_SimpleFile(fp, c);

	return TRUE;
}

gboolean in_unload(int i,struct line *l) {
	GList *g = loaded_scripts;
	printf("GESUCHT:%i\n",i);
	if(l != NULL)
		admin_out(l, "Unload script: #",i);
	printf("List Scripts\n");
	while(g) {
		struct script_thread *p = (struct script_thread *)g->data;
		if((int)p->id == i) {
			Py_EndInterpreter(p->thread);
			loaded_scripts = g_list_remove(loaded_scripts,p);
			return TRUE;
		}
		g = g->next;
	}
	return FALSE;
}

static void init_finish() {
	g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "Loading Python scripts");
	xmlNodePtr cur = xmlConf->xmlChildrenNode;
	while(cur) {
		xmlNodePtr curargs;
		PyObject *args = NULL;
		char *file = NULL;
		if(!xmlIsBlankNode(cur) && !strcmp(cur->name, "load")) {
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
				in_load(file, args);
		}
		cur = cur->next;
	}
	list_scripts(NULL);
}

gboolean init_plugin(struct plugin *p) {
	PyObject *pyxml = NULL;

	xmlConf = p->xmlConf;

	if(!plugin_loaded("admin") && !plugin_loaded("libadmin")) {
		g_warning("admin module required for dynamic load/unload of python scripts");
	} else {
		register_admin_command("PYTHON", in_admin_command);
	}

	//add_motd_hook("python_modt", in_modt_hook);

	// init python
	//Py_SetProgramName(argv[0]);
	Py_Initialize();

	pyxml = PyImport_ImportModule("libxml2mod");
	if(pyxml == NULL) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, "libxml2 python bindings are required");
		return FALSE;

	}


	/* Execute some Python statements (in module __main__) */
	PyRun_SimpleString("import sys\n");
	PyRun_SimpleString("print sys.builtin_module_names\n");
//	PyRun_SimpleString("print ctrlproxy.__dict__\n");
//	PyRun_SimpleString("print sys.modules.keys()\n");

	//cur = xmlFindChildByElementName(p->xmlConf, "load");

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
	del_lose_client_hook("python_modt");

	//g_hash_table_destroy(highlight_backlog); highlight_backlog = NULL;
	return TRUE;
}
