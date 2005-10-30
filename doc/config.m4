# For the docs
AC_PATH_PROG(XSLTPROC, xsltproc)
AC_PATH_PROG(XMLTO, xmlto)
AC_PATH_PROG(XMLLINT, xmllint)
AC_PATH_PROG(DIA, dia)
AC_PATH_PROG(INSTALL, install)

AC_SUBST(DIA)
AC_SUBST(SYNPROG)
AC_SUBST(XSLTPROC)
AC_SUBST(XMLLINT)
AC_SUBST(XMLTO)

DOCS_ENABLED="1"
test -z "$XSLTPROC" || DOCS_ENABLED="1"
