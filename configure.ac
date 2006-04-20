# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT(ctrlproxy, 3.0, [jelmer@vernstok.nl])
PACKAGE=$PACKAGE_NAME
VERSION=$PACKAGE_VERSION
AC_DEFINE_UNQUOTED(PACKAGE,"$PACKAGE", [ Package name])

AC_MSG_CHECKING([whether this is a bzr checkout])
if ! which bzr >/dev/null 2>/dev/null; then
	AC_MSG_RESULT(no)
else 
	REVISION="`bzr revno .`"
	if test -n "$REVISION"; then
		AC_MSG_RESULT($REVISION)
		BZRVERSION="-bzr-r$REVISION"
	else
		AC_MSG_RESULT(no)
	fi
fi

AC_DEFINE_UNQUOTED(VERSION,"$VERSION$BZRVERSION", [ Package version])
AC_SUBST(PACKAGE)
AC_SUBST(VERSION)
AC_CONFIG_SRCDIR([line.c])
AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PATH_PROG(LD,ld)
AC_PATH_PROG(IRCDTORTURE,ircdtorture)

AC_ARG_ENABLE(gcov,
[ --enable-gcov		Enable GCOV support ],
[ WITH_GCOV=1 ], [ WITH_GCOV=0 ])
AC_SUBST(WITH_GCOV)

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_TIME
AC_CHECK_HEADERS(
[stdlib.h string.h unistd.h execinfo.h sys/time.h sys/socket.h])

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

AC_CHECK_FUNCS([gethostbyname gethostname memset strchr strerror strstr uname backtrace_symbols gettimeofday strrchr daemon])

PKG_CHECK_MODULES(COMMON, glib-2.0 gmodule-2.0, , AC_MSG_ERROR([glib is required]))
PKG_CHECK_MODULES(SQLITE, sqlite, [DEFMODULE(linestack_sqlite)])

###############################################################################
# NSS support
###############################################################################
PKG_CHECK_MODULES(NSS, mozilla-nss, 
	[ DEFMODULE(nss, $NSS_LIBS) ], [ echo "Not using mozilla-nss"; ] )

###############################################################################
# OpenSSL support
###############################################################################
PKG_CHECK_MODULES(OPENSSL, openssl, [ DEFMODULE(openssl, $OPENSSL_LIBS) ], [ echo "Not building openssl module" ])

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

###############################################################################
# SWIG support
###############################################################################
AC_PATH_PROG(SWIG,[swig])

AC_PATH_PROG(XSLTPROC, xsltproc)

AC_SUBST(XSLTPROC)

if test -n "$XSLTPROC"
then
	EXTRA_INSTALL_TARGETS="install-doc"
fi

AC_SUBST(EXTRA_INSTALL_TARGETS)

BINS="ctrlproxy$ac_cv_exeext"
AC_SUBST(BINS)
AC_SUBST(MODS_SHARED)

AC_DEFINE(_GNU_SOURCE, 1, [Use GNU extensions])

AC_CONFIG_FILES([Makefile.settings ctrlproxy.pc])
AC_OUTPUT
