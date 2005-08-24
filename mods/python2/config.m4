sinclude(mods/python2/ac_python_devel.m4)

AC_PYTHON_DEVEL()

if test -z "$SWIG"; then
	AC_MSG_WARN([SWIG not found, not building python module])
elif test -z "$PYTHON_VERSION"; then
	AC_MSG_WARN([Python not found, not building python module])
else
	DEFMODULE(python2, $PY_LIBS)
	AC_CONFIG_FILES([mods/python2/Makefile.settings])
fi
