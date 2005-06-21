-include Makefile.settings

CFLAGS+=-DHAVE_CONFIG_H -DSHAREDIR=\"$(cdatadir)\" -DDTD_FILE=\"$(cdatadir)/ctrlproxyrc.dtd\"
CFLAGS+=-ansi -Wall -DMODULESDIR=\"$(modulesdir)\" -DSTRICT_MEMORY_ALLOCS=

SUBDIRS = mods scripts

.PHONY: all clean distclean install install-bin install-dirs install-doc install-data install-mods install-pkgconfig $(SUBDIRS)

all: $(BINS) $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

mods/static.o:: 
	$(MAKE) -C mods static.o

ctrlproxy$(EXEEXT): network.o posix.o client.o cache.o line.o main.o state.o util.o hooks.o linestack.o plugins.o settings.o isupport.o log.o redirect.o gen_config.o $(shell test -n "$(MODS_STATIC)" && echo mods/static.o)
	$(CC) $(LIBS) -rdynamic -o $@ $^

%.$(OBJEXT): %.c
	$(CC) $(CFLAGS) -c $<

configure: autogen.sh configure.in acinclude.m4 $(wildcard mods/*/*.m4)
	./$<

ctrlproxy.pc Makefile.settings: configure Makefile.settings.in ctrlproxy.pc.in
	./$<

install: all install-dirs install-bin install-mods install-data install-pkgconfig install-scripts $(EXTRA_INSTALL_TARGETS)
install-dirs:
	$(INSTALL) -d $(DESTDIR)$(bindir)
	$(INSTALL) -d $(DESTDIR)$(destincludedir)
	$(INSTALL) -d $(DESTDIR)$(modulesdir)
	$(INSTALL) -d $(DESTDIR)$(docdir)
	$(INSTALL) -d $(DESTDIR)$(cdatadir)
	$(INSTALL) -d $(DESTDIR)$(libdir)/pkgconfig

install-bin:
	$(INSTALL) ctrlproxy$(EXEEXT) $(DESTDIR)$(bindir)

install-contrib:
	$(MAKE) -C contrib install

install-doc:
	$(INSTALL) -m 644 ctrlproxy.h $(DESTDIR)$(destincludedir)
	$(INSTALL) AUTHORS $(DESTDIR)$(docdir)
	$(INSTALL) COPYING $(DESTDIR)$(docdir)
	$(INSTALL) TODO $(DESTDIR)$(docdir)
	$(INSTALL) UPGRADING $(DESTDIR)$(docdir)
	$(MAKE) -C doc install PACKAGE_VERSION=$(PACKAGE_VERSION)

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

dist: distclean
	$(MAKE) -C doc dist

distclean: clean
	rm -f build config.h ctrlproxy.pc *.log
	rm -rf autom4te.cache/ config.log config.status
	$(MAKE) -C mods distclean
	$(MAKE) -C testsuite distclean

ctags:
	ctags `find -name "*.c"` `find -name "*.h"`
