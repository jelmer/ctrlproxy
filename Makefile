-include Makefile.settings

CFLAGS+=-DHAVE_CONFIG_H -DLOCALEDIR=\"$(localedir)\" -DSHAREDIR=\"$(cdatadir)\" -DDTD_FILE=\"$(cdatadir)/ctrlproxyrc.dtd\"

CFLAGS+=-ansi -Wall -DMODULESDIR=\"$(modulesdir)\"

.PHONY: all clean distclean install install-bin install-dirs install-doc install-data install-mods install-pkgconfig

all: $(BINS)
	$(MAKE) -C mods all
	$(MAKE) -C scripts all

mods/static.o:
	$(MAKE) -C mods static.o

ctrlproxy$(EXEEXT): server.o line.o main.o state.o util.o hooks.o linestack.o plugins.o config.o mods/static.o
	$(CC) $(LIBS) -rdynamic -o $@ $^

%.$(OBJEXT): %.c
	$(CC) $(CFLAGS) -c $<

Makefile.settings:
	@echo Please run ./configure first, then rerun make

install: all install-dirs install-doc install-bin install-mods install-data install-pkgconfig install-scripts install-locale
install-dirs:
	$(INSTALL) -d $(DESTDIR)$(bindir)
	$(INSTALL) -d $(DESTDIR)$(man1dir)
	$(INSTALL) -d $(DESTDIR)$(man5dir)
	$(INSTALL) -d $(DESTDIR)$(destincludedir)
	$(INSTALL) -d $(DESTDIR)$(modulesdir)
	$(INSTALL) -d $(DESTDIR)$(docdir)
	$(INSTALL) -d $(DESTDIR)$(cdatadir)
	$(INSTALL) -d $(DESTDIR)$(libdir)/pkgconfig

install-locale:
	$(MAKE) -C po install
	
install-bin:
	$(INSTALL) ctrlproxy$(EXEEXT) $(DESTDIR)$(bindir)
	$(INSTALL) ctrlproxy-setup $(DESTDIR)$(bindir)

install-doc:
	$(INSTALL) -m 644 ctrlproxy.h $(DESTDIR)$(destincludedir)
	$(INSTALL) AUTHORS $(DESTDIR)$(docdir)
	$(INSTALL) COPYING $(DESTDIR)$(docdir)
	$(INSTALL) TODO $(DESTDIR)$(docdir)
	$(INSTALL) UPGRADING $(DESTDIR)$(docdir)
ifeq ($(shell test -d doc && echo _),_)
	$(MAKE) -C doc install 
else
	@echo "--------------------------------------------------------------------"
	@echo " It looks like you are compiling ctrlproxy from Subversion"
	@echo " Documentation will not be installed, but is available from "
	@echo " a seperate SVN repository (http://ctrlproxy.vernstok.nl/doc/svn/)."
	@echo "--------------------------------------------------------------------"
endif

install-data:
	$(INSTALL) motd $(DESTDIR)$(cdatadir)
	$(INSTALL) ctrlproxyrc.default $(DESTDIR)$(cdatadir)
	$(INSTALL) ctrlproxyrc.dtd $(DESTDIR)$(cdatadir)

install-mods:
	$(MAKE) -C mods install

install-scripts:
	$(MAKE) -C scripts install

install-pkgconfig:
	$(INSTALL) ctrlproxy.pc $(DESTDIR)$(libdir)/pkgconfig

clean:
	rm -f *.$(OBJEXT) ctrlproxy$(EXEEXT) printstats *~
	$(MAKE) -C mods clean
	$(MAKE) -C testsuite clean

distclean: clean
	rm -f Makefile build config.h ctrlproxy.pc ctrlproxy.spec *.log
	rm -rf autom4te.cache/ config.log config.status
	$(MAKE) -C mods distclean
	$(MAKE) -C testsuite distclean

ctags:
	ctags `find -name "*.c"` `find -name "*.h"`
