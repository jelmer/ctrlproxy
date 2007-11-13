-include Makefile.settings

MODS_SHARED_FILES = $(patsubst %,mods/lib%.$(SHLIBEXT),$(MODS_SHARED))

GCOV = gcov

ifeq ($(WITH_GCOV),1)
GCOV_CFLAGS = -ftest-coverage -fprofile-arcs
GCOV_LIBS = -lgcov
LIBS += $(GCOV_LIBS) $(GCOV_CFLAGS)
endif

LIBS += $(GNUTLS_LIBS)
CFLAGS += $(GNUTLS_CFLAGS)

CFLAGS+=-DHAVE_CONFIG_H -DSHAREDIR=\"$(cdatadir)\" -DDEFAULT_CONFIG_DIR=\"$(DEFAULT_CONFIG_DIR)\" -DHELPFILE=\"$(HELPFILE)\"
CFLAGS+=-ansi -Wall -DMODULESDIR=\"$(modulesdir)\" -DSTRICT_MEMORY_ALLOCS=

.PHONY: all clean distclean install install-bin install-dirs install-doc install-data install-mods install-pkgconfig

all: $(BINS) $(MODS_SHARED_FILES) 

doxygen:
	doxygen

lib_objs = \
	   lib/state.o \
	   lib/client.o

objs = src/network.o \
	   src/posix.o \
	   src/cache.o \
	   src/line.o \
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
	   src/user.o \
	   src/pipes.o \
	   src/help.o \
	   src/repl_backends.o \
	   src/listener.o \
	   src/log_support.o \
	   $(SSL_OBJS)

lib_headers = \
		  lib/state.h \
		  lib/client.h 

headers = src/admin.h \
		  src/ctcp.h \
		  src/ctrlproxy.h \
		  src/hooks.h \
		  src/irc.h \
		  src/line.h \
		  src/linestack.h \
		  src/network.h \
		  src/log_support.h \
		  src/repl.h \
		  src/settings.h \
		  src/ssl.h \
		  src/state.h \
		  src/isupport.h \
		  src/log.h
dep_files = $(patsubst %.o, %.d, $(objs)) $(patsubst %.o, %.d, $(wildcard mods/*.o))

linestack-cmd$(EXEEXT): src/linestack-cmd.o $(objs)
	@echo Linking $@
	@$(LD) $(LIBS) -lreadline -rdynamic -o $@ $^

ctrlproxy$(EXEEXT): src/main.o $(objs) $(LIBIRC)
	@echo Linking $@
	@$(LD) $(LDFLAGS) -rdynamic -o $@ $^ $(LIBS) $(LIBIRC)

ctrlproxy-admin$(EXEEXT): src/admin-cmd.o
	@echo Linking $@
	@$(LD) $(LDFLAGS) -rdynamic -o $@ $^ $(LIBS)

mods/%.o: mods/%.c
	@echo Compiling for shared library $<
	@$(CC) -fPIC -I. -Isrc $(CFLAGS) $(GCOV_CFLAGS) -c $< -o $@

%.o: %.c
	@echo Compiling $<
	@$(CC) -I. -Isrc $(CFLAGS) $(GCOV_CFLAGS) -c $< -o $@

%.d: %.c
	@$(CC) -I. -Isrc -M -MT $(<:.c=.o) $(CFLAGS) $< -o $@

ifeq ($(BZR_CHECKOUT),yes)
configure: autogen.sh configure.ac acinclude.m4 $(wildcard mods/*/*.m4)
	./$<
endif

ctrlproxy.pc Makefile.settings: configure Makefile.settings.in ctrlproxy.pc.in
	./$<

install: all install-dirs install-bin install-header install-mods install-data install-pkgconfig $(EXTRA_INSTALL_TARGETS)
install-dirs:
	$(INSTALL) -d $(DESTDIR)$(modulesdir)

uninstall: uninstall-bin uninstall-header uninstall-mods uninstall-data uninstall-pkgconfig $(patsubst install-%,uninstall-%,$(EXTRA_INSTALL_TARGETS))
uninstall-bin:
	-rm -f $(DESTDIR)$(bindir)/ctrlproxy$(EXEEXT)
	-rmdir $(DESTDIR)$(bindir)

install-bin:
	$(INSTALL) -d $(DESTDIR)$(bindir)
	$(INSTALL) ctrlproxy$(EXEEXT) $(DESTDIR)$(bindir)

uninstall-header:
	-rm -f $(patsubst %,$(DESTDIR)$(destincludedir)/%,$(notdir $(headers)))
	-rmdir $(DESTDIR)$(destincludedir)

install-header::
	$(INSTALL) -d $(DESTDIR)$(destincludedir)
	$(INSTALL) -m 0644 $(headers) $(DESTDIR)$(destincludedir)

install-doc:: doc
	$(INSTALL) -d $(DESTDIR)$(docdir)
	$(INSTALL) -m 0644 UPGRADING $(DESTDIR)$(docdir)
	$(MAKE) -C doc install PACKAGE_VERSION=$(PACKAGE_VERSION)

uninstall-doc: 
	$(MAKE) -C doc uninstall
	rm -f $(DESTDIR)$(docdir)/UPGRADING
	-rmdir $(DESTDIR)$(docdir)

uninstall-data::
	-rm -f $(DESTDIR)$(cdatadir)/motd
	-rm -f $(DESTDIR)$(DEFAULT_CONFIG_DIR)/config
	-rm -f $(DESTDIR)$(DEFAULT_CONFIG_DIR)/networks/admin
	-rmdir $(DESTDIR)$(DEFAULT_CONFIG_DIR)/networks
	-rmdir $(DESTDIR)$(DEFAULT_CONFIG_DIR)
	-rmdir $(DESTDIR)$(cdatadir)

install-data:
	$(INSTALL) -d $(DESTDIR)$(cdatadir)
	$(INSTALL) -m 0644 motd $(DESTDIR)$(cdatadir)
	$(INSTALL) -d $(DESTDIR)$(DEFAULT_CONFIG_DIR)
	$(INSTALL) -d $(DESTDIR)$(DEFAULT_CONFIG_DIR)/networks
	$(INSTALL) -m 0644 config.default $(DESTDIR)$(DEFAULT_CONFIG_DIR)/config
	$(INSTALL) -m 0644 config.admin $(DESTDIR)$(DEFAULT_CONFIG_DIR)/networks/admin

install-mods: all 
	$(INSTALL) -d $(DESTDIR)$(modulesdir)
	$(INSTALL) $(MODS_SHARED_FILES) $(DESTDIR)$(modulesdir)

uninstall-mods:
	-rm -f $(patsubst %,$(DESTDIR)$(modulesdir)/%,$(notdir $(MODS_SHARED_FILES)))
	-rmdir $(DESTDIR)$(modulesdir)

install-pkgconfig:
	$(INSTALL) -d $(DESTDIR)$(libdir)/pkgconfig
	$(INSTALL) -m 0644 ctrlproxy.pc $(DESTDIR)$(libdir)/pkgconfig

uninstall-pkgconfig:
	-rm -f $(DESTDIR)$(libdir)/pkgconfig/ctrlproxy.pc
	-rmdir $(DESTDIR)$(libdir)/pkgconfig

gcov: test
	$(GCOV) -f -p -o src/ src/*.c 

lcov:
	lcov --base-directory `pwd` --directory . --capture --output-file ctrlproxy.info
	genhtml -o coverage ctrlproxy.info

mods/lib%.$(SHLIBEXT): mods/%.o
	@echo Linking $@
	@$(LD) $(LDFLAGS) -fPIC -shared -o $@ $^

LIBIRC = libirc.$(SHLIBEXT).$(PACKAGE_VERSION)
LIBIRC_SOVERSION = 3.0
LIBIRC_SONAME = libirc.$(SHLIBEXT).$(LIBIRC_SOVERSION)

$(LIBIRC): $(lib_objs)
	$(LD) $(LDFLAGS) -Wl,-soname,$(LIBIRC_SONAME) -fPIC -shared -o $@ $^

clean::
	@echo Removing .so files
	@rm -f $(MODS_SHARED_FILES)
	@echo Removing dependency files
	@rm -f $(dep_files)
	@echo Removing object files and executables
	@rm -f src/*.o testsuite/check ctrlproxy$(EXEEXT) testsuite/*.o *~ mods/*.o
	@rm -f linestack-cmd$(EXEEXT) ctrlproxy-admin$(EXEEXT)
	@echo Removing gcov output
	@rm -f *.gcov *.gcno *.gcda 
	@echo Removing test output
	@rm -rf test-*

doc-dist:: configure
	$(MAKE) -C doc dist

dist: configure doc-dist distclean

distclean:: clean 
	rm -f build config.h ctrlproxy.pc *.log
	rm -rf autom4te.cache/ config.log config.status

realclean:: distclean
	@$(MAKE) -C doc clean

ctags:
	ctags -R .

# RFC compliance testing using ircdtorture

TEST_SERVER := localhost
TEST_PORT := 6667

testsuite/ctrlproxyrc.torture: testsuite/ctrlproxyrc.torture.in
	sed -e 's/@SERVER@/$(TEST_SERVER)/;s/@PORT@/$(TEST_PORT)/;' < $< > $@

rfctest: testsuite/ctrlproxyrc.torture
	@$(IRCDTORTURE) -- ./ctrlproxy -d 0 -i TEST -r $<

# Unit tests
check_objs = testsuite/test-cmp.o testsuite/test-user.o \
			 testsuite/test-admin.o testsuite/test-isupport.o \
			 testsuite/test-parser.o testsuite/test-state.o \
			 testsuite/test-util.o testsuite/test-line.o \
			 testsuite/torture.o testsuite/test-linestack.o \
			 testsuite/test-client.o testsuite/test-network.o \
			 testsuite/test-tls.o testsuite/test-redirect.o \
			 testsuite/test-networkinfo.o testsuite/test-ctcp.o \
			 testsuite/test-help.o testsuite/test-nickserv.o

testsuite/check: $(check_objs) $(objs)
	@echo Linking $@
	@$(CC) $(LIBS) -o $@ $^ $(CHECK_LIBS)

CTRLPROXY_MODULESDIR=$(shell pwd)/mods

test:: testsuite/check
	@echo Running testsuite
	@$(VALGRIND) ./testsuite/check

check: test

-include $(dep_files)
