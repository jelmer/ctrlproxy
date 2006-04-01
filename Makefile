-include Makefile.settings

MODS_SHARED_FILES = $(patsubst %,mods/lib%.$(SHLIBEXT),$(MODS_SHARED))

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

all: $(BINS) $(SUBDIRS) $(MODS_SHARED_FILES) 

$(SUBDIRS):
	$(MAKE) -C $@

ctrlproxy$(EXEEXT): network.o posix.o client.o cache.o line.o main.o state.o util.o hooks.o linestack.o plugins.o settings.o isupport.o log.o redirect.o gen_config.o repl.o linestack_file.o ctcp.o motd.o nickserv.o
	@echo Linking $@
	@$(CC) $(LIBS) -rdynamic -o $@ $^

.c.o:
	@echo Compiling $<
	@$(CC) -I. $(CFLAGS) $(GCOV_CFLAGS) -c $< -o $@

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

install-mods: all 
	$(INSTALL) -d $(DESTDIR)$(modulesdir)
	$(INSTALL) $(MODS_SHARED_FILES) $(DESTDIR)$(modulesdir)

install-scripts:
	$(MAKE) -C scripts install

install-pkgconfig:
	$(INSTALL) ctrlproxy.pc $(DESTDIR)$(libdir)/pkgconfig

gcov:
	$(GCOV) -po . *.c 

mods/lib%.so: mods/%.o
	@echo Compiling $<
	$(CC) $(LDFLAGS) -fPIC -shared -o $@ $<

clean::
	rm -f $(MODS_SHARED_FILES)
	rm -f *.$(OBJEXT) ctrlproxy$(EXEEXT) printstats *~
	rm -f *.gcov *.gcno *.gcda
	$(MAKE) -C testsuite clean
	$(MAKE) -C rfctester clean

dist: distclean
	$(MAKE) -C doc dist

distclean:: clean 
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

mods/nss.o: CFLAGS+=$(NSS_CFLAGS)
mods/libnss.$(SHLIBEXT): LDFLAGS+=$(NSS_LDFLAGS)
mods/gnutls.o: CFLAGS+=$(GNUTLS_CFLAGS)
mods/libgnutls.$(SHLIBEXT): LDFLAGS+=$(GNUTLS_LDFLAGS)
mods/openssl.o: CFLAGS+=$(OPENSSL_CFLAGS)
mods/libopenssl.$(SHLIBEXT): LDFLAGS+=$(OPENSSL_LDFLAGS)

# Python specific stuff below this line
mods/python2.o: CFLAGS+=$(PYTHON_CFLAGS)
mods/libpython2.so: LDFLAGS+=$(PYTHON_LDFLAGS)
%_wrap.o: CFLAGS+=$(PYTHON_CFLAGS)

%_wrap.c: %.i
	$(SWIG) -python $*.i

build: ctrlproxy_wrap.c mods/admin_wrap.c mods/listener_wrap.c 
	LDFLAGS="$(LDFLAGS)" CFLAGS="$(CFLAGS)" $(PYTHON) setup.py build

install-python: all
	$(PYTHON) setup.py install --root="$(DESTDIR)"

clean::
	rm -f *_wrap.c *.pyc
	rm -f ctrlproxy.py admin.py listener.py
	$(PYTHON) setup.py clean
	rm -rf build/
