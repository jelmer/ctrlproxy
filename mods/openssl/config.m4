dnl SSL Support
dnl libssl
PKG_CHECK_MODULES(OPENSSL, openssl, [ DEFMODULE(openssl, $OPENSSL_LIBS) ], [ echo "Not building openssl module" ])
AC_CONFIG_FILES([mods/openssl/Makefile])
