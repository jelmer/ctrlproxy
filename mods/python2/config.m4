AC_PATH_PROG(SWIG,swig)

if test "_$SWIG" = _; then
	AC_MSG_WARN([SWIG not found, not building python module])
elif test _$PYTHON_VERSION = _; then
	AC_MSG_WARN([Python not found, not building python module])
else
	MODS_SUBDIRS="$MODS_SUBDIRS python2"
fi

AC_CONFIG_FILES([mods/python2/Makefile])
