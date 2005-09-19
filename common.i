typedef unsigned short guint16;
typedef unsigned long guint32;
typedef unsigned char guint8;
typedef boolean gboolean;

%typemap(python,in) PyObject *PyFunc {
  if (!PyCallable_Check($input)) {
      PyErr_SetString(PyExc_TypeError, "Need a callable object!");
      return NULL;
  }
  $1 = $input;
}

