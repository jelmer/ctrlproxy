include ../Makefile.settings
DOCS = ctrlproxy.1 ctrlproxy_config.5 user-guide.html user-guide/index.html ctrlproxy.1.html ctrlproxy_config.5.html help.txt

.PHONY: all manpages clean distclean

all: $(DOCS)

dist: all

manpages: ctrlproxy.1 ctrlproxy_config.5 

%/index.html: %.xml
	$(XSLTPROC) --stringparam base.dir "$(@D)/" http://docbook.sourceforge.net/release/xsl/current/html/chunk.xsl $<

%.html: %.xml
	$(XSLTPROC) http://docbook.sourceforge.net/release/xsl/current/html/docbook.xsl $< > $@

%.1: %.1.xml 
	$(XSLTPROC) http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $< > $@

%.5: %.5.xml
	$(XSLTPROC) http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $< > $@

help.xml: admin.xml clients.xml

help.txt: help.xml help.xsl 
	$(XSLTPROC) --stringparam extraparanewline "$(EXTRAPARANEWLINE)" --xinclude help.xsl $< | perl -0077 -pe 's/\n\n%/\n%/s; s/_b_/\002/g;' > $@

clean::
	@echo Removing documentation files
	@rm -f *.html *.1 *.5
	@rm -rf user-guide

distclean:: clean

install: all
	$(INSTALL) -d $(DESTDIR)$(mandir)/man1
	$(INSTALL) -d $(DESTDIR)$(mandir)/man5
	$(INSTALL) -d $(DESTDIR)$(docdir)
	$(INSTALL) -m 0644 ctrlproxy.1 $(DESTDIR)$(mandir)/man1
	$(INSTALL) -m 0644 ctrlproxy_config.5 $(DESTDIR)$(mandir)/man5
	$(INSTALL) -m 0644 user-guide.html $(DESTDIR)$(docdir)
