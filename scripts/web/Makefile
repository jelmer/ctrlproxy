-include ../../Makefile.settings
PAGES = $(wildcard pages/*.py) $(wildcard pages/*.css) $(wildcard pages/*.txt)
INCLUDES = $(wildcard includes/*.*)

.PHONY: all install clean distclean

all: 

install: all
	$(INSTALL) -d $(DESTDIR)$(scriptdir)/web
	$(INSTALL) -d $(DESTDIR)$(scriptdir)/web/pages
	$(INSTALL) -d $(DESTDIR)$(scriptdir)/web/includes
	$(INSTALL) server.py $(DESTDIR)$(scriptdir)/web
	$(INSTALL) $(PAGES) $(DESTDIR)$(scriptdir)/web/pages
	$(INSTALL) $(INCLUDES) $(DESTDIR)$(scriptdir)/web/includes

clean:

distclean: clean
	rm -f Makefile
