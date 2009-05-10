# Makefile for ctrlproxy
# Copyright (C) 2002-2009 Jelmer Vernooij <jelmer@samba.org>
# NOTE: This file is *NOT* autogenerated.

include Makefile.settings

BINS += ctrlproxy$(EXEEXT)
SBINS += ctrlproxyd$(EXEEXT)

GCOV = gcov

ifeq ($(WITH_GCOV),1)
CFLAGS += --coverage
LIBS += --coverage
endif

LIBS += $(GNUTLS_LIBS)
CFLAGS += $(GNUTLS_CFLAGS)

CFLAGS+=-DHAVE_CONFIG_H -DDEFAULT_CONFIG_DIR=\"$(DEFAULT_CONFIG_DIR)\" -DHELPFILE=\"$(HELPFILE)\"
CFLAGS+=-ansi -Wall -DMODULESDIR=\"$(modulesdir)\" -DSTRICT_MEMORY_ALLOCS=

LIBIRC_STATIC = libirc.a
LIBIRC = $(LIBIRC_STATIC)

LIBIRC_SHARED = libirc.$(SHLIBEXT).$(PACKAGE_VERSION)
LIBIRC_SOVERSION = 1.0
LIBIRC_SONAME = libirc.$(SHLIBEXT).$(LIBIRC_SOVERSION)

.PHONY: all clean distclean install install-bin install-dirs install-doc install-data install-pkgconfig

all:: $(BINS) $(SBINS)

ifeq ($(HAVE_PYTHON),yes)
check:: check-python
all:: python
endif

experimental:: all 

doxygen:
	doxygen

libircdir = libirc

objs = src/posix.o \
	   src/redirect.o \
	   src/cache.o \
	   src/util.o \
	   src/hooks.o \
	   src/linestack.o \
	   src/plugins.o \
	   src/settings.o \
	   src/log.o \
	   src/client.o \
	   src/gen_config.o \
	   src/repl.o \
	   src/linestack_file.o \
	   src/ctcp_redirect.o \
	   src/ctcp.o \
	   src/motd.o \
	   src/nickserv.o \
	   src/keyfile.o \
	   src/admin.o \
	   src/user.o \
	   src/help.o \
	   src/repl_backends.o \
	   src/listener.o \
	   src/log_support.o \
	   src/log_custom.o \
	   src/log_subst.o \
	   src/auto_away.o \
	   src/network.o \
	   $(CTRLPROXY_SSL_OBJS)
all_objs += $(objs)

libirc_objs = \
	   $(libircdir)/state.o \
	   $(libircdir)/client.o \
	   $(libircdir)/transport.o \
	   $(libircdir)/transport_ioc.o \
	   $(libircdir)/line.o \
	   $(libircdir)/isupport.o \
	   $(libircdir)/connection.o \
	   $(libircdir)/redirect.o \
	   $(libircdir)/url.o \
	   $(libircdir)/util.o \
	   $(libircdir)/listener.o \
	   $(LIBIRC_SSL_OBJS)

libirc_headers = \
		  $(libircdir)/state.h \
		  $(libircdir)/client.h \
		  $(libircdir)/line.h \
		  $(libircdir)/isupport.h \
		  $(libircdir)/irc.h \
		  $(libircdir)/connection.h \
		  $(libircdir)/redirect.h \
		  $(libircdir)/url.h \
		  $(libircdir)/listener.h \
		  $(libircdir)/util.h

pyirc_objs = $(libircdir)/python/irc.o \
			 $(libircdir)/python/transport.o \
			 $(libircdir)/python/state.o

headers = src/admin.h \
		  src/ctcp.h \
		  src/ctrlproxy.h \
		  src/hooks.h \
		  src/linestack.h \
		  src/log_support.h \
		  src/repl.h \
		  src/settings.h \
		  src/ssl.h \
		  src/log.h \
		  src/cache.h
dep_files = $(patsubst %.o, %.d, $(objs))

linestack-cmd$(EXEEXT): src/linestack-cmd.o $(objs) $(LIBIRC)
	@echo Linking $@
	@$(LD) $(LIBS) -lreadline -rdynamic -o $@ $^

ctrlproxy$(EXEEXT): src/main.o $(objs) $(LIBIRC)
	@echo Linking $@
	@$(LD) $(LDFLAGS) -rdynamic -o $@ $^ $(LIBS)

src/settings.o: CFLAGS+=-DSYSCONFDIR=\"${sysconfdir}\"

daemon/main.o: CFLAGS+=-DDEFAULT_CONFIG_FILE=\"${sysconfdir}/ctrlproxyd.conf\" \
	                   -DSSL_CREDENTIALS_DIR=\"${sysconfdir}/ctrlproxy/ssl\" \
					   -DPIDFILE=\"${localstatedir}/run/ctrlproxyd.pid\"

daemon_objs += daemon/main.o daemon/user.o daemon/client.o daemon/backend.o

ctrlproxyd$(EXEEXT): $(daemon_objs) $(objs) $(LIBIRC)
	@echo Linking $@
	@$(LD) $(LDFLAGS) -rdynamic -o $@ $^ $(LIBS)

ctrlproxy-admin$(EXEEXT): src/admin-cmd.o
	@echo Linking $@
	@$(LD) $(LDFLAGS) -rdynamic -o $@ $^ $(LIBS)

%.o: %.c
	@echo Compiling $<
	@$(CC) -I. -I$(libircdir) -Isrc $(CFLAGS) $(GCOV_CFLAGS) -c $< -o $@

%.d: %.c config.h
	@$(CC) -I. -I$(libircdir) -Isrc -M -MT $(<:.c=.o) $(CFLAGS) $(PYTHON_CFLAGS) $< -o $@

# This looks a bit weird but is here to ensure that we never try to 
# run ./autogen.sh outside of bzr checkouts
ifeq ($(BZR_CHECKOUT),yes)
configure: autogen.sh configure.ac acinclude.m4
	./$<
else
configure:
	./autogen.sh
endif

ctrlproxy.pc Makefile.settings config.h: configure Makefile.settings.in ctrlproxy.pc.in
	./$<

install: all install-dirs install-bin install-header install-data install-pkgconfig $(EXTRA_INSTALL_TARGETS)
install-dirs:
	$(INSTALL) -d $(DESTDIR)$(modulesdir)

uninstall: uninstall-bin uninstall-header uninstall-data uninstall-pkgconfig $(patsubst install-%,uninstall-%,$(EXTRA_INSTALL_TARGETS))
uninstall-bin:
	-rm -f $(DESTDIR)$(bindir)/ctrlproxy$(EXEEXT) \
		   $(DESTDIR)$(bindir)/ctrlproxy-admin$(EXEEXT) \
		   $(DESTDIR)$(sbindir)/ctrlproxyd$(EXEEXT)
	-rmdir $(DESTDIR)$(bindir)
	-rmdir $(DESTDIR)$(sbindir)
	-rmdir $(DESTDIR)$(modulesdir)

install-bin:
	$(INSTALL) -d $(DESTDIR)$(bindir)
	$(INSTALL) -d $(DESTDIR)$(sbindir)
	$(INSTALL) $(BINS) $(DESTDIR)$(bindir)
	$(INSTALL) $(SBINS) $(DESTDIR)$(sbindir)

uninstall-header:
	-rm -f $(patsubst %,$(DESTDIR)$(destincludedir)/%,$(notdir $(headers) $(libirc_headers)))
	-rmdir $(DESTDIR)$(destincludedir)

install-header::
	$(INSTALL) -d $(DESTDIR)$(destincludedir)
	$(INSTALL) -m 0644 $(libirc_headers) $(headers) $(DESTDIR)$(destincludedir)

doc::
	$(MAKE) -C doc PACKAGE_VERSION=$(PACKAGE_VERSION)

install-doc:: doc
	$(INSTALL) -d $(DESTDIR)$(docdir)
	$(MAKE) -C doc install PACKAGE_VERSION=$(PACKAGE_VERSION)

uninstall-doc: 
	$(MAKE) -C doc uninstall
	-rmdir $(DESTDIR)$(docdir)

uninstall-data::
	-rm -f $(DESTDIR)$(DEFAULT_CONFIG_DIR)/motd
	-rm -f $(DESTDIR)$(DEFAULT_CONFIG_DIR)/config
	-rmdir $(DESTDIR)$(DEFAULT_CONFIG_DIR)
	-rmdir $(DESTDIR)$(sysconfdir)

install-data:
	$(INSTALL) -d $(DESTDIR)$(sysconfdir)
	$(INSTALL) -d $(DESTDIR)$(DEFAULT_CONFIG_DIR)
	$(INSTALL) -m 0644 motd $(DESTDIR)$(DEFAULT_CONFIG_DIR)
	$(INSTALL) -m 0644 config.default $(DESTDIR)$(DEFAULT_CONFIG_DIR)/config

install-pkgconfig:
	$(INSTALL) -d $(DESTDIR)$(libdir)/pkgconfig
	$(INSTALL) -m 0644 ctrlproxy.pc $(DESTDIR)$(libdir)/pkgconfig

uninstall-pkgconfig:
	-rm -f $(DESTDIR)$(libdir)/pkgconfig/ctrlproxy.pc
	-rmdir $(DESTDIR)$(libdir)/pkgconfig

gcov: check
	$(GCOV) -f -p -o src/ src/*.c 

lcov:
	lcov --base-directory `pwd` --directory . --capture --output-file ctrlproxy.info
	genhtml -o coverage ctrlproxy.info

$(libirc_objs): CFLAGS+=-fPIC

$(LIBIRC_STATIC): $(libirc_objs)
	@echo Linking $@
	@ar -rcs $@ $^

$(LIBIRC_SHARED): $(libirc_objs)
	$(LD) -shared $(LDFLAGS) -Wl,-soname,$(LIBIRC_SONAME) -o $@ $^

%.$(SHLIBEXT):
	$(LD) -shared $(LDFLAGS) -o $@ $^

cscope.out::
	cscope -b -R

clean::
	@echo Removing object files and executables
	@rm -f src/*.o $(libircdir)/*.o daemon/*.o python/*.o testsuite/check ctrlproxy$(EXEEXT) testsuite/*.o *~
	@rm -f linestack-cmd$(EXEEXT) ctrlproxy-admin$(EXEEXT)
	@rm -f ctrlproxyd$(EXEEXT)
	@rm -f $(LIBIRC_STATIC) $(LIBIRC_SHARED)
	@rm -f mods/*.$(SHLIBEXT) mods/*.o
	@echo Removing gcov output
	@rm -f *.gcov *.gcno *.gcda  */*.gcda */*.gcno */*.gcov
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

# Python specific stuff below this line
mods/python.o python/ctrlproxy.o: CFLAGS+=$(PYTHON_CFLAGS)
mods/python.o python/ctrlproxy.o: CFLAGS+=-fPIC
mods/libpython.so: mods/python.o python/ctrlproxy.o $(pyirc_objs)
mods/libpython.so: LDFLAGS+=$(PYTHON_LDFLAGS)

.PRECIOUS: python/irc.c python/ctrlproxy.c

$(pyirc_objs): CFLAGS+=$(PYTHON_CFLAGS) -fPIC
libirc/python/irc.$(SHLIBEXT): $(pyirc_objs) $(LIBIRC)
libirc/python/irc.$(SHLIBEXT): LDFLAGS+=$(PYTHON_LDFLAGS) $(LIBS)

ifeq ($(HAVE_PYTHON),yes)
all_objs += $(pyirc_objs) mods/python.o python/ctrlproxy.o
endif

python:: libirc/python/irc.$(SHLIBEXT) mods/libpython.$(SHLIBEXT)

check-python:: libirc/python/irc.$(SHLIBEXT)
	PYTHONPATH=libirc/python trial tests.test_irc

install-python: all
	$(PYTHON) setup.py install --root="$(DESTDIR)"

clean::
	@rm -f python/tests/*.pyc
	@rm -f libirc/python/tests/*.pyc
	@rm -f libirc/python/*.$(SHLIBEXT) libirc/python/*.o
#	$(PYTHON) setup.py clean

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
			 testsuite/test-help.o testsuite/test-nickserv.o \
			 testsuite/test-url.o testsuite/test-motd.o \
			 testsuite/test-log-subst.o testsuite/test-transport.o

testsuite/check: $(check_objs) $(objs) $(LIBIRC)
	@echo Linking $@
	@$(CC) $(LIBS) -o $@ $^ $(CHECK_LIBS)

check:: testsuite/check
	@echo Running testsuite
	@$(DEBUGGER) ./testsuite/check $(CHECK_OPTIONS)

check-nofork:: 
	$(MAKE) check CHECK_OPTIONS=-nsv

check-gdb: 
	$(MAKE) check-nofork DEBUGGER="gdb --args"

clean::
	@echo Removing dependency files
	@rm -f $(dep_files)

examples:: example/libfoo.$(SHLIBEXT) example/libirc-simple

example/libfoo.$(SHLIBEXT): example/foo.o
example/foo.o: CFLAGS+=-I$(libircdir)

example/libirc-simple: example/irc_simple.o $(LIBIRC)
	$(CC) -o $@ $^

example/irc_simple.o: CFLAGS+=-I$(libircdir)

-include $(dep_files)
