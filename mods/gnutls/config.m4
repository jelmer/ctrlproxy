dnl gnutls
AC_PATH_PROG(gnutls_config, libgnutls-config, no)

GNUTLS_LIBS=""

if test "$gnutls_config" != "no" && test "`libgnutls-config --version | cut -f 1 -d .`" = "1"; then
	GNUTLS_CFLAGS="`$gnutls_config --cflags`"
	GNUTLS_LIBS="`$gnutls_config --libs`"
	MODS_SUBDIRS="$MODS_SUBDIRS gnutls"
	AC_CHECK_HEADERS([gnutls/gnutls.h])
fi

AC_SUBST(GNUTLS_CFLAGS)
AC_SUBST(GNUTLS_LIBS)
AC_CONFIG_FILES([mods/gnutls/Makefile])
