libircdir ?= .
LIBIRC_STATIC = libirc.a
LIBIRC = $(LIBIRC_STATIC)

LIBIRC_SHARED = libirc.$(SHLIBEXT).$(PACKAGE_VERSION)
LIBIRC_SOVERSION = 1.0
LIBIRC_SONAME = libirc.$(SHLIBEXT).$(LIBIRC_SOVERSION)

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
	   $(libircdir)/linestack.o \
	   $(LIBIRC_SSL_OBJS)

libirc_install_headers = \
		  $(libircdir)/state.h \
		  $(libircdir)/client.h \
		  $(libircdir)/line.h \
		  $(libircdir)/isupport.h \
		  $(libircdir)/irc.h \
		  $(libircdir)/connection.h \
		  $(libircdir)/redirect.h \
		  $(libircdir)/url.h \
		  $(libircdir)/listener.h \
		  $(libircdir)/util.h \
		  $(libircdir)/linestack.h \

pyirc_objs = $(libircdir)/python/irc.o \
			 $(libircdir)/python/transport.o \
			 $(libircdir)/python/linestack.o \
			 $(libircdir)/python/state.o

$(libirc_objs): CFLAGS+=-fPIC

$(LIBIRC_STATIC): $(libirc_objs)
	@echo Linking $@
	@ar -rcs $@ $^

$(LIBIRC_SHARED): $(libirc_objs)
	@echo Linking $@
	@$(LD) $(LDFLAGS) -shared -Wl,-soname,$(LIBIRC_SONAME) -o $@ $^ $(LIBS)

clean::
	@rm -f $(libircdir)/*.o
	@rm -f $(LIBIRC_STATIC) $(LIBIRC_SHARED)

$(pyirc_objs): CFLAGS+=$(PYTHON_CFLAGS) -fPIC
$(libircdir)/python/irc.$(SHLIBEXT): $(pyirc_objs) $(LIBIRC)
	@echo Linking $@: $(LDFLAGS) $(LIBS) $(PYTHON_LIBS) 
	@$(CC) $(LDFLAGS) -shared -o $@ $^ $(PYTHON_LIBS) $(LIBS)

$(libircdir)/tools/linestack-cmd$(EXEEXT): $(libircdir)/tools/linestack-cmd.o \
	                                       $(LIBIRC)
	@echo Linking $@: $(LD) $(LDFLAGS) -rdynamic -o $@ $^ $(LIBS)
	@$(LD)  $(LDFLAGS) -rdynamic -o $@ $^ $(LIBS)

clean::
	@rm -f linestack-cmd$(EXEEXT)

python:: $(libircdir)/python/irc.$(SHLIBEXT)

check-python:: $(libircdir)/python/irc.$(SHLIBEXT)
	PYTHONPATH=$(abspath $(libircdir)/python) $(PYTHON) $(abspath $(libircdir)/python/tests/test_irc.py)
	PYTHONPATH=$(abspath $(libircdir)/python) $(PYTHON) -m unittest tests.test_irc

uninstall-header::
	-rm -f $(patsubst %,$(DESTDIR)$(destincludedir)/%,$(notdir $(libirc_install_headers)))
	-rmdir $(DESTDIR)$(destincludedir)

install-header::
	$(INSTALL) -d $(DESTDIR)$(destincludedir)
	$(INSTALL) -m 0644 $(libirc_install_headers) $(DESTDIR)$(destincludedir)

clean::
	@rm -f $(libircdir)/python/tests/*.pyc
	@rm -f $(libircdir)/python/*.$(SHLIBEXT) $(libircdir)/python/*.o
