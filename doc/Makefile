include Makefile.settings
DOCS = ctrlproxy.1 ctrlproxyrc.5 ctrlproxy.pdf ctrlproxy.html $(MOD_MANPAGES)

.PHONY: all most manpages htmlmanpages lint clean distclean

most: $(DOCS)
	touch $@
all: $(DOCS) ctrlproxy.pdf ctrlproxy.ps
everything: $(DOCS) htmlmanpages
dist: most
	rm -rf autom4te.cache

manpages: ctrlproxy.1 ctrlproxyrc.5 $(MOD_MANPAGES)
htmlmanpages: $(MOD_MANPAGES_HTML)

%.7ctrlproxy.xml: %.mod.xml module-man.xsl
	$(XSLTPROC) -o $@ module-man.xsl $<

%.xml: %.mod.xml module.xsl
	$(XSLTPROC) -o $@ module.xsl $<

%.7ctrlproxy.html: %.7ctrlproxy.xml
	$(XSLTPROC) -o $@ http://docbook.sourceforge.net/release/xsl/current/html/docbook.xsl $<

%.html: %.xml $(MODS) overview.png overview2.png
	$(XSLTPROC) -o $@ http://docbook.sourceforge.net/release/xsl/current/html/docbook.xsl $<

%.ps: %.xml $(MODS) overview.eps overview2.eps
	$(XMLTO) ps $<

lint: ctrlproxy.xml $(MODS)
	$(XMLLINT) --valid --noout --xinclude $<

%.1: %.1.xml 
	$(XSLTPROC) http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $< > $@

%.5: %.5.xml
	$(XSLTPROC) http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $< > $@

%.pdf: %.xml overview.pdf overview2.pdf
	$(XMLTO) pdf $<

%.png: %.dia
	$(DIA) -n -e $@ $<

%.eps: %.dia
	$(DIA) -n -e $@ $<

clean: 
	rm -f *.html *.pdf $(DOCS) $(MODS) overview{2,}.{eps,png} $(MOD_MANPAGES_INTER)

distclean: clean
	rm -f Makefile.settings

install: most
	$(INSTALL) -d $(DESTDIR)$(man1dir)
	$(INSTALL) -d $(DESTDIR)$(man5dir)
	$(INSTALL) -d $(DESTDIR)$(man7dir)
	$(INSTALL) -d $(DESTDIR)$(docdir)
	$(INSTALL) ctrlproxy.1 $(DESTDIR)$(man1dir)
	$(INSTALL) ctrlproxyrc.5 $(DESTDIR)$(man5dir)
	$(INSTALL) $(MOD_MANPAGES) $(DESTDIR)$(man7dir)
	$(INSTALL) ctrlproxy.pdf $(DESTDIR)$(docdir)
	$(INSTALL) ctrlproxy.html $(DESTDIR)$(docdir)

