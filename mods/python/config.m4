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
	PY_VER=`$pythonpath -c 'import distutils.sysconfig; print distutils.sysconfig.get_config_vars("VERSION")[0];'`
	PY_LIB=`$pythonpath -c 'import distutils.sysconfig; print distutils.sysconfig.get_config_vars("LIBDEST")[0];'`
	PY_INC=`$pythonpath -c 'import distutils.sysconfig; print distutils.sysconfig.get_config_vars("INCLUDEPY")[0];'`
	$pythonpath -c "import sys; map(int,sys.version[:3].split('.')) >= [2,2] or sys.exit(1)"
	changequote([, ])dnl
	AC_MSG_RESULT($PY_VER)
	if test "$?" != "1"; then
		AC_MSG_CHECKING(Python compile flags)
		PY_PREFIX=`$pythonpath -c 'import sys; print sys.prefix'`
		PY_EXEC_PREFIX=`$pythonpath -c 'import sys; print sys.exec_prefix'`
		if test -f $PY_INC/Python.h; then
			PY_LIBS="-L$PY_LIB/config -lpython$PY_VER -lpthread -lutil"
			PY_CFLAGS="-I$PY_INC"
			MODS="$MODS libpython.so"
			AC_MSG_RESULT(ok)
		else
			AC_MSG_RESULT([can't find Python.h])
		fi
	else
		AC_MSG_RESULT([too old. Only 2.2 or above is supported.])
	fi
fi

AC_SUBST(PY_CFLAGS)
AC_SUBST(PY_LIBS)
