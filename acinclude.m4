AC_DEFUN([DEFMODULE],
[AC_PREREQ(2.57)dnl
	AC_MSG_CHECKING([how to build $1])
	if test "$module_default" = "shared"; then
		MODS_SHARED="$MODS_SHARED $1"
		AC_MSG_RESULT(shared)
	else if test "$module_default" = "static"; then
		LIBS="$LIBS $2"
		MODS_STATIC="$MODS_STATIC $1"
		AC_MSG_RESULT(static)
	else
		AC_MSG_RESULT(not)
	fi
fi
])
