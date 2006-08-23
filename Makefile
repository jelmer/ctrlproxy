-include Makefile.settings

VPATH = src:testsuite

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

objs = src/network.o \
	   src/posix.o \
	   src/client.o \
	   src/cache.o \
	   src/line.o \
	   src/state.o \
	   src/util.o \
	   src/hooks.o \
	   src/linestack.o \
	   src/plugins.o \
	   src/settings.o \
	   src/isupport.o \
	   src/log.o \
	   src/redirect.o \
	   src/gen_config.o \
	   src/repl.o \
	   src/linestack_file.o \
	   src/ctcp.o \
	   src/motd.o \
	   src/nickserv.o \
	   src/admin.o \
	   src/user.o
dep_files = $(patsubst %.o, %.d, $(objs)) $(patsubst %.o, %.d, $(wildcard mods/*.o))

ctrlproxy$(EXEEXT): src/main.o $(objs)
	@echo Linking $@
	@$(CC) $(LIBS) -rdynamic -o $@ $^

.c.o:
	@echo Compiling $<
	@$(CC) -I. -Isrc $(CFLAGS) $(GCOV_CFLAGS) -c $< -o $@

%.d: %.c
	@$(CC) -I. -Isrc -M -MG -MP -MT $(<:.c=.o) $(CFLAGS) $< -o $@

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
	$(INSTALL) -m 644 src/ctrlproxy.h $(DESTDIR)$(destincludedir)
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

gcov: check
	$(GCOV) -p -o src/ src/*.c 

mods/lib%.$(SHLIBEXT): mods/%.o
	@echo Linking $@
	@$(CC) $(LDFLAGS) -fPIC -shared -o $@ $^

clean::
	@echo Removing .so files
	@rm -f $(MODS_SHARED_FILES)
	@echo Removing dependency files
	@rm -f $(dep_files)
	@echo Removing object files and executables
	@rm -f src/*.o testsuite/check ctrlproxy$(EXEEXT) testsuite/*.o *~ mods/*.o
	@echo Removing gcov output
	@rm -f *.gcov *.gcno *.gcda 

dist: distclean
	$(MAKE) -C doc dist

distclean:: clean 
	rm -f build config.h ctrlproxy.pc *.log
	rm -rf autom4te.cache/ config.log config.status

ctags:
	ctags -R .

mods/gnutls.o mods/tlscert.o: CFLAGS+=$(GNUTLS_CFLAGS)
mods/libgnutls.$(SHLIBEXT): mods/gnutls.o mods/tlscert.o
mods/libgnutls.$(SHLIBEXT): LDFLAGS+=$(GNUTLS_LDFLAGS)

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

# Unit tests
check_objs = testsuite/test-cmp.o testsuite/test-user.o \
			 testsuite/test-isupport.o testsuite/test-parser.o \
			 testsuite/test-state.o testsuite/test-util.o \
			 testsuite/test-line.o testsuite/torture.o \
			 testsuite/test-linestack.o
testsuite/check: $(check_objs) $(objs)
	@echo Linking $@
	@$(CC) $(LIBS) -o $@ $^ -lcheck

CTRLPROXY_MODULESDIR=$(shell pwd)/mods

test: testsuite/check
	@echo Running testsuite
	@$(VALGRIND) ./testsuite/check

-include $(dep_files)
