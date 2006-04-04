include Makefile.settings
DOCS = ctrlproxy.1 ctrlproxyrc.5 user-guide/index.html

.PHONY: all most manpages htmlmanpages lint clean distclean

most: $(DOCS)
all: $(DOCS) ctrlproxy.pdf ctrlproxy.ps
everything: $(DOCS) htmlmanpages

dist: most

manpages: ctrlproxy.1 ctrlproxyrc.5 

%/index.html: %.xml
	$(XMLTO) -o $(@D) html $<

%.html: %.xml
	$(XMLTO) html-nochunks $<

%.ps: %.xml 
	$(XMLTO) ps $<

lint: ctrlproxy.xml 
	$(XMLLINT) --valid --noout --xinclude $<

%.1: %.1.xml 
	$(XSLTPROC) http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $< > $@

%.5: %.5.xml
	$(XSLTPROC) http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $< > $@

%.pdf: %.xml
	$(XMLTO) pdf $<

clean: 
	rm -f *.html *.pdf $(DOCS)

distclean: clean
	rm -f Makefile.settings

install: most
	$(INSTALL) -d $(DESTDIR)$(man1dir)
	$(INSTALL) -d $(DESTDIR)$(man5dir)
	$(INSTALL) -d $(DESTDIR)$(docdir)
	$(INSTALL) ctrlproxy.1 $(DESTDIR)$(man1dir)
	$(INSTALL) ctrlproxyrc.5 $(DESTDIR)$(man5dir)
	$(INSTALL) ctrlproxy.pdf $(DESTDIR)$(docdir)
	$(INSTALL) ctrlproxy.html $(DESTDIR)$(docdir)
