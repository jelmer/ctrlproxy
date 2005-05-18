if test "${enable_python+set}" = set; then
	AC_PATH_PROG(SWIG,swig)

	if test "_$SWIG" = _; then
		AC_MSG_WARN([SWIG not found, not building python module])
	elif test _$PYTHON_VERSION = _; then
		AC_MSG_WARN([Python not found, not building python module])
	else
		PY_CFLAGS="-I`$PYTHON -c 'import distutils.sysconfig; print distutils.sysconfig.get_python_inc()'`"
		PY_LIBS="-L`$PYTHON -c 'import distutils.sysconfig; print distutils.sysconfig.get_python_lib(standard_lib=1)'`"
		PY_LIBS="$PY_LIBS `$PYTHON -c 'import distutils.sysconfig; print distutils.sysconfig.get_config_var("BLDLIBRARY")'`"
		DEFMODULE(python2, $PY_LIBS)
		AC_SUBST(PY_LIBS)
		AC_SUBST(PY_CFLAGS)
	fi

	AC_CONFIG_FILES([mods/python2/Makefile])
fi
