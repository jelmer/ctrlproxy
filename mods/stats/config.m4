LIB_PCRE=""
AC_CHECK_LIB(pcre, pcre_compile, [ LIB_PCRE="-lpcre";  ] )
AC_CHECK_HEADERS([tdb.h regex.h pcre.h])
AC_CHECK_LIB(tdb, tdb_open, [ MODS_SUBDIRS="$MODS_SUBDIRS stats";] )
AC_SUBST(LIB_PCRE)
AC_CONFIG_FILES([mods/stats/Makefile])
