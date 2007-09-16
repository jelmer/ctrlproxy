# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT(ctrlproxy, 3.0.4, [jelmer@vernstok.nl])
PACKAGE=$PACKAGE_NAME
VERSION=$PACKAGE_VERSION
AC_DEFINE_UNQUOTED(PACKAGE,"$PACKAGE", [ Package name])

AC_SUBST(BZR_CHECKOUT)
BZR_CHECKOUT=no
AC_MSG_CHECKING([whether this is a bzr checkout])
if ! which bzr >/dev/null 2>/dev/null; then
	AC_MSG_RESULT(no)
else 
	BZR_CHECKOUT=yes
	REVISION="`bzr revno .`"
	if test -n "$REVISION"; then
		AC_MSG_RESULT($REVISION)
		BZRVERSION="-`bzr nick .`bzr-r$REVISION"
	else
		AC_MSG_RESULT(no)
	fi
fi

AC_DEFINE_UNQUOTED(VERSION,"$VERSION$BZRVERSION", [ Package version])
AC_SUBST(PACKAGE)
AC_SUBST(VERSION)
AC_CONFIG_SRCDIR([src/line.c])
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
[stdlib.h string.h unistd.h execinfo.h sys/time.h sys/socket.h netinet/in.h netinet/in6.h])

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

PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES(COMMON, glib-2.0 >= 2.6.0 gmodule-2.0)

PKG_CHECK_MODULES(CHECK, check, [], [ echo -n "" ])

###############################################################################
# GNUTLS support
###############################################################################
PKG_CHECK_MODULES(GNUTLS, gnutls, [
		AC_DEFINE(HAVE_GNUTLS, 1, [Whether GNUTLS is available])
		SSL_OBJS="src/gnutls.o src/tlscert.o"
		AC_SUBST(SSL_OBJS)
		], [ AC_MSG_WARN([GNUTLS not found, SSL will not be available]) ])

AC_CHECK_LIB(readline, readline, [ 
	AC_DEFINE(HAVE_READLINE, 1, [Whether readline is available])
	BINS="$BINS linestack-cmd$ac_cv_exeext ctrlproxy-admin$ac_cv_exeext" 
	LIBS="$LIBS -lreadline"
])

###############################################################################
# IPv6 support
###############################################################################
AC_CACHE_CHECK([for IPv6], ac_cv_lib_ipv6, [
AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <sys/types.h>
#include <sys/socket.h>
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NETINET_IN6_H
#include <netinet/in6.h>
#endif
]],
[[
 struct sockaddr_in6 sin6;
 int s;

 s = socket(AF_INET6, SOCK_DGRAM, 0);

 sin6.sin6_family = AF_INET6;
 sin6.sin6_port = htons(17);
 sin6.sin6_addr = in6addr_any;
 bind(s, (struct sockaddr *)&sin6, sizeof(sin6));
]])],
[ac_cv_lib_ipv6=yes],
[ac_cv_lib_ipv6=no])])

if test "$ac_cv_lib_ipv6" = yes; then
  AC_DEFINE(HAVE_IPV6, 1, [Define if you have IPv6.])
fi

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

BINS="$BINS ctrlproxy$ac_cv_exeext"
AC_SUBST(BINS)
AC_SUBST(MODS_SHARED)

AC_DEFINE(_GNU_SOURCE, 1, [Use GNU extensions])

AC_CONFIG_FILES([Makefile.settings ctrlproxy.pc])
AC_OUTPUT
