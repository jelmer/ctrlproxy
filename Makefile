-include Makefile.settings

MODS_SHARED_FILES = $(patsubst %,mods/lib%.$(SHLIBEXT),$(MODS_SHARED))

GCOV = gcov

ifeq ($(WITH_GCOV),1)
GCOV_CFLAGS = -ftest-coverage -fprofile-arcs
GCOV_LIBS = -lgcov
LIBS += $(GCOV_LIBS) $(GCOV_CFLAGS)
endif

CFLAGS+=-DHAVE_CONFIG_H -DSHAREDIR=\"$(cdatadir)\" -DDEFAULT_CONFIG_DIR=\"$(DEFAULT_CONFIG_DIR)\"
CFLAGS+=-ansi -Wall -DMODULESDIR=\"$(modulesdir)\" -DSTRICT_MEMORY_ALLOCS=

.PHONY: all clean distclean install install-bin install-dirs install-doc install-data install-mods install-pkgconfig

all: $(BINS) $(MODS_SHARED_FILES) 

objs = network.o posix.o client.o cache.o line.o state.o util.o hooks.o linestack.o plugins.o settings.o isupport.o log.o redirect.o gen_config.o repl.o linestack_file.o ctcp.o motd.o nickserv.o admin.o
dep_files = $(patsubst %.o, %.d, $(objs)) $(patsubst %.c, %.d, $(wildcard mods/*.c))

ctrlproxy$(EXEEXT): main.o $(objs)
	@echo Linking $@
	@$(CC) $(LIBS) -rdynamic -o $@ $^

.c.o:
	@echo Compiling $<
	@$(CC) -I. $(CFLAGS) $(GCOV_CFLAGS) -c $< -o $@

%.d: %.c
	@$(CC) -I. -M -MG -MP -MT $(<:.c=.o) $(CFLAGS) $< -o $@

configure: autogen.sh configure.ac acinclude.m4 $(wildcard mods/*/*.m4)
	./$<

ctrlproxy.pc Makefile.settings: configure Makefile.settings.in ctrlproxy.pc.in
	./$<

install: all install-dirs install-bin install-mods install-data install-pkgconfig $(EXTRA_INSTALL_TARGETS)
install-dirs:
	$(INSTALL) -d $(DESTDIR)$(bindir)
	$(INSTALL) -d $(DESTDIR)$(destincludedir)
	$(INSTALL) -d $(DESTDIR)$(modulesdir)
	$(INSTALL) -d $(DESTDIR)$(docdir)
	$(INSTALL) -d $(DESTDIR)$(cdatadir)
	$(INSTALL) -d $(DESTDIR)$(libdir)/pkgconfig

install-bin:
	$(INSTALL) ctrlproxy$(EXEEXT) $(DESTDIR)$(bindir)
	$(INSTALL) scripts/upgrade.py $(DESTDIR)$(bindir)/ctrlproxy-upgrade 

install-doc: doc
	$(INSTALL) -m 644 ctrlproxy.h $(DESTDIR)$(destincludedir)
	$(INSTALL) AUTHORS $(DESTDIR)$(docdir)
	$(INSTALL) COPYING $(DESTDIR)$(docdir)
	$(INSTALL) BUGS $(DESTDIR)$(docdir)
	$(INSTALL) UPGRADING $(DESTDIR)$(docdir)
	$(MAKE) -C doc install PACKAGE_VERSION=$(PACKAGE_VERSION)

install-data:
	$(INSTALL) motd $(DESTDIR)$(cdatadir)
	$(INSTALL) -d $(DESTDIR)$(DEFAULT_CONFIG_DIR)
	$(INSTALL) -d $(DESTDIR)$(DEFAULT_CONFIG_DIR)/networks
	$(INSTALL) config.default $(DESTDIR)$(DEFAULT_CONFIG_DIR)/config
	$(INSTALL) config.admin $(DESTDIR)$(DEFAULT_CONFIG_DIR)/networks/admin

install-mods: all 
	$(INSTALL) -d $(DESTDIR)$(modulesdir)
	$(INSTALL) $(MODS_SHARED_FILES) $(DESTDIR)$(modulesdir)

install-pkgconfig:
	$(INSTALL) ctrlproxy.pc $(DESTDIR)$(libdir)/pkgconfig

gcov:
	$(GCOV) -po . *.c 

mods/lib%.$(SHLIBEXT): mods/%.o
	@echo Linking $@
	@$(CC) $(LDFLAGS) -fPIC -shared -o $@ $^

clean::
	@echo Removing .so files
	@rm -f $(MODS_SHARED_FILES)
	@echo Removing dependency files
	@rm -f $(dep_files)
	@echo Removing object files and executables
	@rm -f *.$(OBJEXT) ctrlproxy$(EXEEXT) *~ mods/*.o
	@echo Removing gcov output
	@rm -f *.gcov *.gcno *.gcda 

dist: distclean
	$(MAKE) -C doc dist

distclean:: clean 
	rm -f build config.h ctrlproxy.pc *.log
	rm -rf autom4te.cache/ config.log config.status

ctags:
	ctags -R .

mods/nss.o: CFLAGS+=$(NSS_CFLAGS)
mods/libnss.$(SHLIBEXT): LDFLAGS+=$(NSS_LDFLAGS)
mods/gnutls.o: CFLAGS+=$(GNUTLS_CFLAGS)
mods/libgnutls.$(SHLIBEXT): LDFLAGS+=$(GNUTLS_LDFLAGS)
mods/openssl.o: CFLAGS+=$(OPENSSL_CFLAGS)
mods/libopenssl.$(SHLIBEXT): LDFLAGS+=$(OPENSSL_LDFLAGS)
mods/liblinestack_sqlite.$(SHLIBEXT): LDFLAGS+=$(SQLITE_LDFLAGS)
mods/linestack_sqlite.o: CFLAGS+=$(SQLITE_CFLAGS)

# Python specific stuff below this line
mods/python2.o ctrlproxy_wrap.o: CFLAGS+=$(PYTHON_CFLAGS)
mods/libpython2.so: mods/python2.o ctrlproxy_wrap.o
mods/libpython2.so: LDFLAGS+=$(PYTHON_LDFLAGS)

%_wrap.c: %.i
	$(SWIG) -python $*.i

build: ctrlproxy_wrap.c mods/listener_wrap.c 
	LDFLAGS="$(LDFLAGS)" CFLAGS="$(CFLAGS)" $(PYTHON) setup.py build

install-python: all
	$(PYTHON) setup.py install --root="$(DESTDIR)"

clean::
	rm -f *_wrap.c *.pyc
	rm -f ctrlproxy.py listener.py
#	$(PYTHON) setup.py clean
	rm -rf build/

# RFC compliance testing using ircdtorture

TEST_SERVER := localhost
TEST_PORT := 6667

testsuite/ctrlproxyrc.torture: testsuite/ctrlproxyrc.torture.in
	sed -e 's/@SERVER@/$(TEST_SERVER)/;s/@PORT@/$(TEST_PORT)/;' < $< > $@

rfctest: testsuite/ctrlproxyrc.torture
	@$(IRCDTORTURE) -- ./ctrlproxy -d 0 -i TEST -r $<

# Regular testsuite

$(patsubst testsuite/%.c,testsuite/lib%.$(SHLIBEXT),$(wildcard testsuite/test-*.c)): testsuite/lib%.$(SHLIBEXT): testsuite/%.o $(objs)

testsuite/lib%.$(SHLIBEXT): 
	@echo Linking $@
	@$(CC) $(LIBS) $(CFLAGS) -shared -o $@ $^

test: testsuite/torture $(patsubst testsuite/%.c,testsuite/lib%.$(SHLIBEXT),$(wildcard testsuite/test-*.c)) 
	@echo Running testsuite
	@$(VALGRIND) ./$< $(patsubst %,./%,$(filter testsuite/libtest-%.$(SHLIBEXT),$^))

testsuite/torture: testsuite/torture.o 
	@echo Linking $@
	@$(CC) $(LIBS) -o $@ $^ 

-include $(dep_files)
