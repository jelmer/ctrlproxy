-include ../Makefile.settings
CFLAGS += -Wall -I.. -DHAVE_CONFIG_H
MODS_SHARED_SUBDIRS = $(foreach mod, $(MODS_SHARED), $(shell test -d $(mod) && echo $(mod)))
MODS_SHARED_FILES = $(foreach mod, $(MODS_SHARED), $(shell test -f $(mod).c && echo lib$(mod).so))
MODS_STATIC_SUBDIRS = $(foreach mod, $(MODS_STATIC), $(shell test -d $(mod) && echo $(mod)))
MODS_STATIC_SUBDIRS_FILES = $(foreach mod, $(MODS_STATIC_SUBDIRS), $(mod)/$(mod).o)
MODS_STATIC_FILES = $(foreach mod, $(MODS_STATIC), $(shell test -f $(mod).c && echo $(mod).o)) $(MODS_STATIC_SUBDIRS_FILES)
INCLUDEFILES = $(wildcard *.h)

.PHONY: all install clean distclean

all: $(MODS_SHARED_FILES) 
	for I in $(MODS_SHARED_SUBDIRS); do $(MAKE) -C $$I $@; done

install: all
	$(INSTALL) -d $(DESTDIR)$(modulesdir)
ifneq "$(MODS_SHARED_FILES)" ""
	$(INSTALL) $(MODS_SHARED_FILES) $(DESTDIR)$(modulesdir)
endif
	$(INSTALL) -m 644 $(INCLUDEFILES) $(DESTDIR)$(destincludedir)
	for I in $(MODS_SHARED_SUBDIRS); do $(MAKE) -C $$I $@; done

$(MODS_STATIC_SUBDIRS_FILES):
	$(MAKE) -C $(@D) $(@F)

static.o: $(MODS_STATIC_FILES) 
	$(LD) -r -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -Dinit_plugin=$*_init -Dfini_plugin=$*_fini -Dname_plugin=$*_name -Dload_config=$*_load_config -Dsave_config=$*_save_config -c $<

lib%.so: %.c
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $<

clean:
	for I in $(MODS_SHARED_SUBDIRS) $(MODS_STATIC_SUBDIRS); do $(MAKE) -C $$I $@; done
	rm -f $(MODS_SHARED_FILES) $(MODS_STATIC_FILES) *.o *.so

distclean: clean
	for I in $(MODS_SHARED_SUBDIRS) $(MODS_STATIC_SUBDIRS); do $(MAKE) -C $$I $@; done
	rm -f Makefile
