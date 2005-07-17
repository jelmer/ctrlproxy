sinclude(ac_python_devel.m4)

AC_PYTHON_DEVEL
AC_PATH_PROG(SWIG,swig)

if test "_$SWIG" = _; then
	AC_MSG_WARN([SWIG not found, not building python module])
elif test _$PYTHON_VERSION = _; then
	AC_MSG_WARN([Python not found, not building python module])
else
	DEFMODULE(python2, $PY_LIBS)
	AC_CONFIG_FILES([mods/python2/Makefile.settings])
fi
