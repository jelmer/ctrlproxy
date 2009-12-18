# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT(ctrlproxy, 3.0.9, [jelmer@samba.org])
PACKAGE=$PACKAGE_NAME
VERSION=$PACKAGE_VERSION
AC_DEFINE_UNQUOTED(PACKAGE,"$PACKAGE", [ Package name])

AC_SUBST(BZR_CHECKOUT)
BZR_CHECKOUT=no
AC_MSG_CHECKING([whether this is a bzr checkout])
if which bzr >/dev/null 2>/dev/null; then
	BZR_CHECKOUT=yes
	REVISION="`bzr revno .`"
	if test -n "$REVISION"; then
		AC_MSG_RESULT($REVISION)
		BZRVERSION="~bzr$REVISION"
	else
		AC_MSG_RESULT(no)
	fi
else 
	AC_MSG_RESULT(no)
fi

AC_DEFINE_UNQUOTED(VERSION,"$VERSION$BZRVERSION", [ Package version])
AC_SUBST(PACKAGE)
AC_SUBST(VERSION)
AC_CONFIG_SRCDIR([src/admin-cmd.c])
AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PATH_PROG(IRCDTORTURE,ircdtorture)

if test "$GCC" = "yes"; then
	CFLAGS="$CFLAGS -ansi -Wall"
	DYNAMIC="-rdynamic"
fi
AC_SUBST(GCC)
AC_SUBST(DYNAMIC)

AC_ARG_ENABLE(gcov,
[ --enable-gcov		Enable GCOV support ],
[ WITH_GCOV=1 ], [ WITH_GCOV=0 ])
AC_SUBST(WITH_GCOV)


# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_TIME
AC_CHECK_HEADERS(
[stdlib.h string.h unistd.h execinfo.h sys/time.h sys/socket.h syslog.h])

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

AC_CHECK_FUNC(gethostbyname, , AC_CHECK_LIB(nsl, gethostbyname))
AC_CHECK_FUNC(setsockopt, , AC_CHECK_LIB(socket, setsockopt))
AC_CHECK_FUNCS([gethostname memset strchr strerror strstr uname backtrace_symbols gettimeofday strrchr daemon fork writev])
AC_CHECK_FUNC(gcry_control, , AC_CHECK_LIB(gcrypt, gcry_control))

PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.6.0 gmodule-2.0 >= 2.6.0)
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)
PKG_CHECK_MODULES(CHECK, check, [], [ echo -n "" ])

###############################################################################
# GNUTLS support
###############################################################################
PKG_CHECK_MODULES(GNUTLS, gnutls, [
		AC_DEFINE(HAVE_GNUTLS, 1, [Whether GNUTLS is available])
		LIBIRC_SSL_OBJS="libirc/gnutls.o"
		CTRLPROXY_SSL_OBJS="src/tlscert.o"
		AC_SUBST(LIBIRC_SSL_OBJS)
		AC_SUBST(CTRLPROXY_SSL_OBJS)
		], [ AC_MSG_WARN([GNUTLS not found, SSL will not be available]) ])

AC_CHECK_LIB(readline, readline, [ 
	AC_DEFINE(HAVE_READLINE, 1, [Whether readline is available])
	BINS="$BINS libirc/tools/linestack-cmd$ac_cv_exeext ctrlproxy-admin$ac_cv_exeext" 
	LIBS="$LIBS -lreadline"
])

###############################################################################
# Python support
###############################################################################
AC_PATH_PROG(PYTHON, python)
AC_PATH_PROG(PYTHON_CONFIG, python-config)

HAVE_PYTHON=no
AC_SUBST(HAVE_PYTHON)
if test -z "$PYTHON_CONFIG"; then
	AC_MSG_WARN([Python not found, not building python module])
else
	DEFMODULE(python, `$PYTHON_CONFIG --libs`)
	HAVE_PYTHON=yes
fi
###############################################################################
# GSSAPI support
###############################################################################
AC_PATH_PROG(KRB5_CONFIG, krb5-config, no)
if test $KRB5_CONFIG != no && $KRB5_CONFIG --libs gssapi >/dev/null 2>&1; then
	LIBS="$LIBS `$KRB5_CONFIG --libs gssapi`"
	CFLAGS="$CFLAGS `$KRB5_CONFIG --cflags gssapi`"
fi
AC_CHECK_FUNC(gss_display_status,
		AC_DEFINE(HAVE_GSSAPI, 1, [have GSSAPI]),
		AC_CHECK_LIB(gss, gss_display_status,
				LIBS="$LIBS -lgss"; AC_DEFINE(HAVE_GSSAPI, 1, [have GSSAPI])))
AC_CHECK_FUNC(gsskrb5_register_acceptor_identity, 
		AC_DEFINE(HAVE_GSSKRB5_REGISTER_ACCEPTOR_IDENTITY, 1, [Whether gsskrb5_register_acceptor_identity is provided by the GSSAPI library]))
AC_CHECK_HEADERS([gssapi.h gssapi/gssapi.h])

AC_PATH_PROG(XSLTPROC, xsltproc)

AC_SUBST(XSLTPROC)

if test -n "$XSLTPROC"
then
	EXTRA_INSTALL_TARGETS="install-doc"
fi

AC_SUBST(EXTRA_INSTALL_TARGETS)

AC_SUBST(BINS)

AC_DEFINE(_GNU_SOURCE, 1, [Use GNU extensions])

AC_CONFIG_FILES([Makefile.settings ctrlproxy.pc])
AC_OUTPUT
