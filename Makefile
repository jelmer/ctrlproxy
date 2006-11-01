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
	   src/user.o \
	   $(SSL_OBJS)
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
	$(INSTALL) -m 0644 src/ctrlproxy.h $(DESTDIR)$(destincludedir)
	$(INSTALL) -m 0644 AUTHORS $(DESTDIR)$(docdir)
	$(INSTALL) -m 0644 COPYING $(DESTDIR)$(docdir)
	$(INSTALL) -m 0644 BUGS $(DESTDIR)$(docdir)
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
