include ../Makefile.settings
DOCS = ctrlproxy.1 ctrlproxyrc.5 user-guide.html user-guide/index.html

.PHONY: all manpages clean distclean

all: $(DOCS)

dist: all

manpages: ctrlproxy.1 ctrlproxyrc.5 

%/index.html: %.xml
	$(XSLTPROC) --stringparam base.dir "$(@D)/" http://docbook.sourceforge.net/release/xsl/current/html/chunk.xsl $<

%.html: %.xml
	$(XSLTPROC) http://docbook.sourceforge.net/release/xsl/current/html/docbook.xsl $< > $@

%.1: %.1.xml 
	$(XSLTPROC) http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $< > $@

%.5: %.5.xml
	$(XSLTPROC) http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $< > $@

clean::
	rm -f *.html *.1 *.5
	rm -rf user-guide

distclean:: clean
	rm -f Makefile.settings

install: all
	$(INSTALL) -d $(DESTDIR)$(mandir)/man1
	$(INSTALL) -d $(DESTDIR)$(mandir)/man5
	$(INSTALL) -d $(DESTDIR)$(docdir)
	$(INSTALL) ctrlproxy.1 $(DESTDIR)$(mandir)/man1
	$(INSTALL) ctrlproxyrc.5 $(DESTDIR)$(mandir)/man5
	$(INSTALL) user-guide.html $(DESTDIR)$(docdir)
