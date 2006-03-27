-include ../Makefile.settings
CFLAGS += -Wall -I.. -DHAVE_CONFIG_H
MODS_SHARED_SUBDIRS = $(foreach mod, $(MODS_SHARED), $(shell test -d $(mod) && echo $(mod)))
MODS_SHARED_FILES = $(foreach mod, $(MODS_SHARED), $(shell test -f $(mod).c && echo lib$(mod).so))
INCLUDEFILES = $(wildcard *.h)

.PHONY: all install clean distclean $(MODS_SHARED_SUBDIRS)

all: $(MODS_SHARED_FILES) $(MODS_SHARED_SUBDIRS)

$(MODS_SHARED_SUBDIRS): 
	$(MAKE) -C $@

install-%:
	$(MAKE) -C $* install

clean-%:
	$(MAKE) -C $* clean

distclean-%:
	$(MAKE) -C $* distclean

install: all $(addprefix install-,$(MODS_SHARED_SUBDIRS))
	$(INSTALL) -d $(DESTDIR)$(modulesdir)
ifneq "$(MODS_SHARED_FILES)" ""
	$(INSTALL) $(MODS_SHARED_FILES) $(DESTDIR)$(modulesdir)
endif
	$(INSTALL) -m 644 $(INCLUDEFILES) $(DESTDIR)$(destincludedir)

%.o: %.c
	$(CC) $(CFLAGS) -Dplugin=plugin_$* -c $<

lib%.so: %.c
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $<

clean: $(addprefix clean-,$(MODS_SHARED_SUBDIRS))
	rm -f $(MODS_SHARED_FILES) *.o *.so

distclean: clean $(addprefix distclean-,$(MODS_SHARED_SUBDIRS))
