-include ../Makefile.settings
SUBDIRS = web
SCRIPTS = 

.PHONY: all install clean distclean $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

clean-%:
	$(MAKE) -C $* clean

install-%:
	$(MAKE) -C $* install

install: all $(addprefix install-,$(SUBDIRS))
	$(INSTALL) -d $(DESTDIR)$(scriptdir)
	test -z "$(SCRIPTS)" || $(INSTALL) $(SCRIPTS) $(DESTDIR)$(scriptdir)

clean: $(addprefix clean-,$(SUBDIRS))

distclean: clean
