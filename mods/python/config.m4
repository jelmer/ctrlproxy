dnl *********************************************************************
dnl ** PYTHON ***********************************************************
dnl *********************************************************************
test -f ${srcdir-.}/doc/ctrlproxy.1 && DOCE=""

AC_PATH_PROG(pythonpath, python)

if test "_$pythonpath" = _ ; then
	python=no
else
	AC_MSG_CHECKING(Python version)
	changequote(<<, >>)dnl
	PY_VER=`$pythonpath -c 'import distutils.sysconfig; print distutils.sysconfig.get_config_var("VERSION");'`
	PY_LIB=`$pythonpath -c 'import distutils.sysconfig; print distutils.sysconfig.get_config_var("LIBDIR");'`
	PY_INC=`$pythonpath -c 'import distutils.sysconfig; print distutils.sysconfig.get_config_var("INCLUDEPY");'`
	$pythonpath -c "import sys; map(int,sys.version[:3].split('.')) >= [2,3] or sys.exit(1)"
	changequote([, ])dnl
	AC_MSG_RESULT($PY_VER)
	if test "$?" != "1"; then
		AC_MSG_CHECKING(Python compile flags)
		PY_PREFIX=`$pythonpath -c 'import sys; print sys.prefix'`
		PY_EXEC_PREFIX=`$pythonpath -c 'import sys; print sys.exec_prefix'`
		if test -f $PY_INC/Python.h; then
			PY_LIBS=`$pythonpath -c 'import distutils.sysconfig; print distutils.sysconfig.get_config_var("BLDLIBRARY"),distutils.sysconfig.get_config_var("LIBS");'`
			PY_CFLAGS=`$pythonpath -c 'import distutils.sysconfig; print distutils.sysconfig.get_config_var("CFLAGS");'`
			PY_CFLAGS="$PY_CFLAGS -I$PY_INC"
			DEFMODULE(python)
			AC_MSG_RESULT(ok)
		else
			AC_MSG_RESULT([can't find Python.h])
		fi
	else
		AC_MSG_RESULT([too old. Only 2.3 or above is supported.])
	fi
fi

AC_SUBST(PY_CFLAGS)
AC_SUBST(PY_LIBS)
AC_CONFIG_FILES([mods/python/Makefile])

