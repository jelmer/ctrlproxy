# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT(ctrlproxy, 2.7, [jelmer@vernstok.nl])
PACKAGE=$PACKAGE_NAME
VERSION=$PACKAGE_VERSION
AC_DEFINE_UNQUOTED(PACKAGE,"$PACKAGE", [ Package name])

AC_MSG_CHECKING([whether this is a SVN checkout])
if ! which svnversion >/dev/null 2>/dev/null; then
	AC_MSG_RESULT(no)
else 
	REVISION="`svnversion .`"
	if test "$REVISION" = "exported"; then
		AC_MSG_RESULT(no)
	else
		AC_MSG_RESULT($REVISION)
		SVNVERSION="-SVN-r$REVISION"
	fi
fi

AC_DEFINE_UNQUOTED(VERSION,"$VERSION$SVNVERSION", [ Package version])
AC_SUBST(PACKAGE)
AC_SUBST(VERSION)
AC_CONFIG_SRCDIR([line.c])
AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PATH_PROG(LD,ld)

AC_ARG_ENABLE(gcov,
[ --enable-gcov		Enable GCOV support ],
[ WITH_GCOV=1 ], [ WITH_GCOV=0 ])
AC_SUBST(WITH_GCOV)

AC_ARG_WITH(module-default,
[ --with-module-default={shared,static,not} Default module output type ],
[ 
 	case "$withval" in
		shared|static|not) module_default="$withval" ;;
		*)
		AC_MSG_ERROR([Unknown module output type $withval])
		;;
	esac
],
[ module_default="shared" ]
)


BINS="ctrlproxy$ac_cv_exeext"
DEFMODULE(log_irssi)
DEFMODULE(ctcp)
DEFMODULE(auto_away)
DEFMODULE(admin)
DEFMODULE(noticelog)
DEFMODULE(nickserv)
DEFMODULE(log_custom)
DEFMODULE(antiflood)
DEFMODULE(repl_simple)
DEFMODULE(repl_command)
DEFMODULE(repl_highlight)
DEFMODULE(linestack_file)
DEFMODULE(report_time)
DEFMODULE(repl_none)
DEFMODULE(motd_file)
DEFMODULE(repl_lastdisconnect)
DEFMODULE(autosave)
DEFMODULE(debug)
DEFMODULE(socks)
DEFMODULE(listener)
DEFMODULE(autosend)

# Checks for libraries.
AC_CHECK_LIB([popt], [poptGetArg], , AC_MSG_WARN([Popt is strongly advised]))

PKG_CHECK_MODULES(COMMON, libxml-2.0 glib-2.0 gmodule-2.0, , AC_MSG_ERROR([libxml and glib are required]))

###############################################################################
# NSS support
###############################################################################
PKG_CHECK_MODULES(NSS, mozilla-nss, 
	[ DEFMODULE(nss, $NSS_LIBS) ], [ echo "Not using mozilla-nss"; ] )
AC_CONFIG_FILES([mods/nss/Makefile.settings])

###############################################################################
# OpenSSL support
###############################################################################
PKG_CHECK_MODULES(OPENSSL, openssl, [ DEFMODULE(openssl, $OPENSSL_LIBS) ], [ echo "Not building openssl module" ])
AC_CONFIG_FILES([mods/openssl/Makefile.settings])

###############################################################################
# GNUTLS support
###############################################################################
AC_PATH_PROG(gnutls_config, libgnutls-config, no)

GNUTLS_LIBS=""

if test "$gnutls_config" != "no" && test "`libgnutls-config --version | cut -f 1 -d .`" = "1"; then
	GNUTLS_CFLAGS="`$gnutls_config --cflags`"
	GNUTLS_LIBS="`$gnutls_config --libs`"
	DEFMODULE(gnutls, $GNUTLS_LIBS)
	AC_CHECK_HEADERS([gnutls/gnutls.h])
fi

AC_SUBST(GNUTLS_CFLAGS)
AC_SUBST(GNUTLS_LIBS)
AC_CONFIG_FILES([mods/gnutls/Makefile.settings])

###############################################################################
# SWIG support
###############################################################################
AC_PATH_PROG(SWIG,[swig])

###############################################################################
# Python support
###############################################################################
AC_PYTHON_DEVEL()

if test -z "$SWIG"; then
	AC_MSG_WARN([SWIG not found, not building python module])
elif test -z "$PYTHON_VERSION"; then
	AC_MSG_WARN([Python not found, not building python module])
else
	DEFMODULE(python2, $PY_LIBS)
	AC_CONFIG_FILES([mods/python2/Makefile.settings])
fi

sinclude(doc/config.m4)
AC_CONFIG_FILES(doc/Makefile.settings)

AC_DEFINE_UNQUOTED(STATIC_MODULES,[$STATIC_MODULES_LIST],[List of statically built modules])
AC_DEFINE_UNQUOTED(STATIC_MODULE_DECLARES,[$STATIC_MODULE_DECLARES],[Declarations for the static modules])

AC_SUBST(EXTRA_INSTALL_TARGETS)
test -f doc/most && EXTRA_INSTALL_TARGETS="install-doc"

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_TIME
AC_CHECK_HEADERS(
[stdlib.h string.h unistd.h popt.h execinfo.h sys/time.h sys/socket.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_STRUCT_TM
AC_TYPE_UID_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_TYPE_SIGNAL

AC_SUBST(BINS)
AC_SUBST(MODS_STATIC)
AC_SUBST(MODS_SHARED)
AC_CHECK_FUNCS([gethostbyname gethostname memset mkdir strcasecmp strchr strdup strerror strncasecmp strstr uname backtrace_symbols gettimeofday strrchr daemon])

DESTINCLUDEDIR="$includedir/ctrlproxy-2"
AC_SUBST(DESTINCLUDEDIR)

AC_CONFIG_FILES([Makefile.settings ctrlproxy.pc])
AC_OUTPUT