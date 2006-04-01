-include Makefile.settings

MODS_SHARED_SUBDIRS = $(foreach mod, $(MODS_SHARED), $(shell test -d mods/$(mod) && echo mods/$(mod)))
MODS_SHARED_FILES = $(foreach mod, $(MODS_SHARED), $(shell test -f mods/$(mod).c && echo mods/lib$(mod).so))

GCOV = gcov

ifeq ($(WITH_GCOV),1)
GCOV_CFLAGS = -ftest-coverage -fprofile-arcs
GCOV_LIBS = -lgcov
LIBS += $(GCOV_LIBS) $(GCOV_CFLAGS)
endif

CFLAGS+=-DHAVE_CONFIG_H -DSHAREDIR=\"$(cdatadir)\" -DDTD_FILE=\"$(cdatadir)/ctrlproxyrc.dtd\"
CFLAGS+=-ansi -Wall -DMODULESDIR=\"$(modulesdir)\" -DSTRICT_MEMORY_ALLOCS=

SUBDIRS = scripts testsuite rfctester

.PHONY: all clean distclean install install-bin install-dirs install-doc install-data install-mods install-pkgconfig $(SUBDIRS)

all: $(BINS) $(SUBDIRS) $(MODS_SHARED_FILES) $(MODS_SHARED_SUBDIRS)

$(MODS_SHARED_SUBDIRS): 
	$(MAKE) -C $@

$(SUBDIRS):
	$(MAKE) -C $@

ctrlproxy$(EXEEXT): network.o posix.o client.o cache.o line.o main.o state.o util.o hooks.o linestack.o plugins.o settings.o isupport.o log.o redirect.o gen_config.o repl.o linestack_file.o ctcp.o motd.o nickserv.o
	@echo Linking $@
	@$(CC) $(LIBS) -rdynamic -o $@ $^

%.$(OBJEXT): %.c
	@echo Compiling $<
	@$(CC) $(CFLAGS) $(GCOV_CFLAGS) -c $<

configure: autogen.sh configure.ac acinclude.m4 $(wildcard mods/*/*.m4)
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

install-mods: all $(addprefix install-,$(MODS_SHARED_SUBDIRS))
	$(INSTALL) -d $(DESTDIR)$(modulesdir)
	$(INSTALL) $(MODS_SHARED_FILES) $(DESTDIR)$(modulesdir)

install-scripts:
	$(MAKE) -C scripts install

install-pkgconfig:
	$(INSTALL) ctrlproxy.pc $(DESTDIR)$(libdir)/pkgconfig

gcov:
	$(GCOV) -po . *.c 

install-mods/%:
	$(MAKE) -C mods/$* install

clean-mods/%:
	$(MAKE) -C mods/$* clean

distclean-mods/%:
	$(MAKE) -C mods/$* distclean

mods/lib%.so: mods/%.c
	@echo Compiling $<
	@$(CC) -I. $(CFLAGS) -fPIC -shared -o $@ $<

clean: $(addprefix clean-,$(MODS_SHARED_SUBDIRS))
	rm -f $(MODS_SHARED_FILES) mods/*.so
	rm -f *.$(OBJEXT) ctrlproxy$(EXEEXT) printstats *~
	rm -f *.gcov *.gcno *.gcda
	$(MAKE) -C testsuite clean
	$(MAKE) -C rfctester clean

dist: distclean
	$(MAKE) -C doc dist

distclean: clean $(addprefix distclean-,$(MODS_SHARED_SUBDIRS))
	rm -f build config.h ctrlproxy.pc *.log
	rm -rf autom4te.cache/ config.log config.status
	$(MAKE) -C testsuite distclean
	$(MAKE) -C rfctester distclean

test: all
	$(MAKE) -C testsuite test

rfctest: all
	$(MAKE) -C testsuite rfctest

ctags:
	ctags -R .
