%module(package="ctrlproxy") admin
%{
#include "../admin.h"
%}
%ignore register_admin_command;
%include "common.i";
%rename(AdminCommand) admin_command;
%immutable admin_command::help;
%immutable admin_command::help_details;
%include "../admin.h";

#ifdef SWIGPYTHON

%typemap(python, in) PyObject *pyfunc {
	    if (!PyCallable_Check($input)) {
			        PyErr_SetString(PyExc_TypeError, "Need a callable object!");
				        return NULL;
		    }
		    $1 = $input;
}

%inline %{

static void pyadmin_cmd_handler (const struct client *c, char **args, void *userdata)
{
	PyObject *arglist;
	PyObject *argobj;
	PyObject *pyfunc = userdata, *ret;
	int i;
	arglist = PyList_New(0);
	for (i = 0; args[i]; i++) {
		PyList_Append(arglist, PyString_FromString(args[i]));
	}
	argobj = Py_BuildValue("(O)", arglist);
	ret = PyEval_CallObject(pyfunc, argobj);
	if (ret == NULL) {
		PyErr_Print();
		PyErr_Clear();
	}
	Py_DECREF(argobj);
}

void py_register_admin_command(const char *name, PyObject *pyfunc, const char *help, const char *help_details) 
{
		struct admin_command *adm;

		Py_INCREF(pyfunc);

		adm = g_new0(struct admin_command, 1);

		adm->name = g_strdup(name);
		adm->help = g_strdup(help);
		adm->help_details = g_strdup(help_details);
		adm->handler = pyadmin_cmd_handler;
		adm->userdata = pyfunc;

		register_admin_command(adm);
}

%}
%rename(register_admin_command) py_register_admin_command;
void py_register_admin_command(const char *name, PyObject *pyfunc, const char *help=NULL, const char *help_details=NULL);
#endif
