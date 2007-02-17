-include Makefile.settings

VPATH = src:testsuite

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
	   src/user.o \
	   src/pipes.o \
	   src/help.o \
	   $(SSL_OBJS)

headers = src/admin.h \
		  src/client.h \
		  src/ctcp.h \
		  src/ctrlproxy.h \
		  src/hooks.h \
		  src/irc.h \
		  src/line.h \
		  src/linestack.h \
		  src/network.h \
		  src/repl.h \
		  src/settings.h \
		  src/ssl.h \
		  src/state.h \
		  src/log.h
dep_files = $(patsubst %.o, %.d, $(objs)) $(patsubst %.o, %.d, $(wildcard mods/*.o))

linestack-cmd$(EXEEXT): src/linestack-cmd.o $(objs)
	@echo Linking $@
	@$(CC) $(LIBS) -lreadline -rdynamic -o $@ $^

ctrlproxy$(EXEEXT): src/main.o $(objs)
	@echo Linking $@
	@$(CC) $(LIBS) -rdynamic -o $@ $^

mods/%.o: mods/%.c
	@echo Compiling for shared library $<
	@$(CC) -fPIC -I. -Isrc $(CFLAGS) $(GCOV_CFLAGS) -c $< -o $@

%.o: %.c
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

install-doc: doc
	$(INSTALL) -m 0644 $(headers) $(DESTDIR)$(destincludedir)
	$(INSTALL) -m 0644 UPGRADING $(DESTDIR)$(docdir)
	$(MAKE) -C doc install PACKAGE_VERSION=$(PACKAGE_VERSION)

install-data:
	$(INSTALL) -m 0644 motd $(DESTDIR)$(cdatadir)
	$(INSTALL) -d $(DESTDIR)$(DEFAULT_CONFIG_DIR)
	$(INSTALL) -d $(DESTDIR)$(DEFAULT_CONFIG_DIR)/networks
	$(INSTALL) -m 0644 config.default $(DESTDIR)$(DEFAULT_CONFIG_DIR)/config
	$(INSTALL) -m 0644 config.admin $(DESTDIR)$(DEFAULT_CONFIG_DIR)/networks/admin

install-mods: all 
	$(INSTALL) -d $(DESTDIR)$(modulesdir)
	$(INSTALL) $(MODS_SHARED_FILES) $(DESTDIR)$(modulesdir)

install-pkgconfig:
	$(INSTALL) -m 0644 ctrlproxy.pc $(DESTDIR)$(libdir)/pkgconfig

gcov: test
	$(GCOV) -f -p -o src/ src/*.c 

lcov:
	lcov --base-directory `pwd` --directory . --capture --output-file ctrlproxy.info
	genhtml -o coverage ctrlproxy.info

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
	@rm -f linestack-cmd$(EXEEXT)
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
			 testsuite/test-help.o
testsuite/check: $(check_objs) $(objs)
	@echo Linking $@
	@$(CC) $(LIBS) -o $@ $^ -lcheck

CTRLPROXY_MODULESDIR=$(shell pwd)/mods

test: testsuite/check
	@echo Running testsuite
	@$(VALGRIND) ./testsuite/check

-include $(dep_files)
