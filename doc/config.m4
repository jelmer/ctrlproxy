DOCBUILDE="#"
DOCINSTALLE="#"

AC_ARG_ENABLE(docs,
[ --enable-docs		 Build the documentation ],
[ test x$enableval = xyes && DOCBUILDE=""; ])

test -f doc/ctrlproxy.1 && DOCINSTALLE=""

DB2LATEXPATH=""
AC_ARG_WITH(db2latex,
[ --with-db2latex=path	Use db2latex from specified path ],
[ DB2LATEXPATH="$withval" ])

for I in /usr/share/sgml/docbook/stylesheet/xsl/db2latex/latex/docbook.xsl
do
	AC_MSG_CHECKING([for db2latex in $I])
	if test -f "$I"; then
		DB2LATEXPATH="$I"
		AC_MSG_RESULT([yes])
	else
		AC_MSG_RESULT([no])
	fi
done

NODB2LATEX=""
test -z "$DB2LATEXPATH" && NODB2LATEX="#"

# For the docs
AC_PATH_PROG(XSLTPROC, xsltproc)
AC_PATH_PROG(XMLTO, xmlto)
AC_PATH_PROG(DIA, dia)
AC_PATH_PROG(EPSTOPDF, epstopdf)
AC_PATH_PROG(PDFLATEX, pdflatex)

AC_SUBST(PDFLATEX)
AC_SUBST(DB2LATEXPATH)
AC_SUBST(NODB2LATEX)
AC_SUBST(DIA)
AC_SUBST(EPSTOPDF)
AC_SUBST(DOCBUILDE)
AC_SUBST(DOCINSTALLE)
AC_CONFIG_FILES([doc/Makefile])
