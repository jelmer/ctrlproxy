AC_DEFUN([DEFMODULE],
[AC_PREREQ(2.57)dnl
AC_MSG_CHECKING([how to build $1])
case "$module_default" in
	shared)
		MODS_SHARED="$MODS_SHARED $1"
		AC_MSG_RESULT(shared)
	;;
	static)
		LIBS="$LIBS $2"
		MODS_STATIC="$MODS_STATIC $1"
		STATIC_MODULES_LIST="&plugin_$1,$STATIC_MODULES_LIST"
		STATIC_MODULE_DECLARES="extern struct plugin plugin_$1;$STATIC_MODULE_DECLARES"
		AC_MSG_RESULT(static)
	;;
	*) AC_MSG_RESULT(not) ;;
esac
])
