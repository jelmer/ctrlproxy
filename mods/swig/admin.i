%module(package="ctrlproxy") admin
%{
#include "../admin.h"
%}
%ignore register_admin_command;
%include "common.i";
%include "../admin.h";

#ifdef SWIGPYTHON
%{
static void admin_command_callback (char **args, struct line *l, void *userdata)
{
   PyObject *func, *arglist;
   PyObject *result;
   
   func = (PyObject *) userdata;
   arglist = Py_BuildValue("()"); /* FIXME: Put arguments in there */
   result = PyEval_CallObject(func,arglist);
   Py_DECREF(arglist);
   Py_XDECREF(result);
}

static void register_command(char *cmd, PyObject *PyFunc, const char *help, const char *help_details)
{
	register_admin_command(cmd, admin_command_callback, help, help_details, PyFunc);
	Py_INCREF(PyFunc);
}

%}
void register_command(char *cmd, PyObject *PyFunc, const char *help = NULL, const char *help_details = NULL);

#endif
