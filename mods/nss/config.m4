dnl Mozilla NSS
PKG_CHECK_MODULES(NSS, mozilla-nss, 
	[ DEFMODULE(nss, $NSS_LIBS) ], [ echo "Not using mozilla-nss"; ] )
AC_CONFIG_FILES([mods/nss/Makefile])
