SOCKET_OBJS="socket.o"
SSL_LIB=""

AC_CHECK_HEADERS([arpa/inet.h netdb.h netinet/in.h sys/socket.h fcntl.h sys/ioctl.h])
AC_CHECK_FUNCS([dup2 socket])

AC_CHECK_LIB(crypto, DES_set_key)

dnl SSL Support
dnl libssl
AC_CHECK_HEADERS([openssl/ssl.h])
AC_CHECK_LIB(ssl, SSL_connect, [ SOCKET_OBJS="$SOCKET_OBJS openssl.o"; SSL_LIB="-lssl" ] )

dnl gnutls
AC_PATH_PROG(gnutls_config, libgnutls-config, no)

MAJVER=`libgnutls-config --version | cut -f 1 -d .`

if test "$gnutls_config" != "no" && $MAJVER = "1"; then
	CFLAGS="$CFLAGS `$gnutls_config --cflags`"
	SOCKET_OBJS="$SOCKET_OBJS gnutls.o"
	SSL_LIB="$SSL_LIB `$gnutls_config --libs`"
	AC_CHECK_HEADERS([gnutls/openssl.h])
fi

AC_SUBST(SOCKET_OBJS)
AC_SUBST(SSL_LIB)
MODS_SUBDIRS="$MODS_SUBDIRS socket"
AC_CONFIG_FILES([mods/socket/Makefile])
