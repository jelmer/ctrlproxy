include ../Makefile.settings
DOCS = ctrlproxy.1 ctrlproxy-admin.1 ctrlproxy_config.5 user-guide.html user-guide/index.html ctrlproxy.1.html ctrlproxy-admin.1.html ctrlproxy_config.5.html help.txt

.PHONY: all manpages clean distclean

all: $(DOCS)

dist: all

manpages: ctrlproxy.1 ctrlproxy_config.5 ctrlproxy-admin.1

%.tmp.xml: %.xml
	$(XSLTPROC) --xinclude docbook.xsl $< > $@

%/index.html: %.tmp.xml
	$(XSLTPROC) --stringparam base.dir "$(@D)/" http://docbook.sourceforge.net/release/xsl/current/html/chunk.xsl $<

%.html: %.tmp.xml
	$(XSLTPROC) --xinclude http://docbook.sourceforge.net/release/xsl/current/html/docbook.xsl $< > $@

%.1: %.1.tmp.xml 
	$(XSLTPROC) http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $< > $@

%.5: %.5.tmp.xml
	$(XSLTPROC) http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $< > $@

help.xml: admin.xml clients.xml

help.txt: help.xml help.xsl 
	$(XSLTPROC) --stringparam extraparanewline "$(EXTRAPARANEWLINE)" --xinclude help.xsl $< | perl -0077 -pe 's/\n\n%/\n%/s; s/_b_/\002/g;' > $@

clean::
	@echo Removing documentation files
	@rm -f *.html *.1 *.5 help.txt
	@rm -rf user-guide
	@rm -f *.tmp.xml

distclean:: clean

install: all
	$(INSTALL) -d $(DESTDIR)$(mandir)/man1
	$(INSTALL) -d $(DESTDIR)$(mandir)/man5
	$(INSTALL) -d $(DESTDIR)$(docdir)
	$(INSTALL) -m 0644 ctrlproxy.1 $(DESTDIR)$(mandir)/man1
	$(INSTALL) -m 0644 ctrlproxy-admin.1 $(DESTDIR)$(mandir)/man1
	$(INSTALL) -m 0644 help.txt $(DESTDIR)$(HELPFILE)
	$(INSTALL) -m 0644 ctrlproxy_config.5 $(DESTDIR)$(mandir)/man5
	$(INSTALL) -m 0644 user-guide.html $(DESTDIR)$(docdir)

uninstall: 
	rm -f $(DESTDIR)$(mandir)/man1/ctrlproxy.1 
	rm -f $(DESTDIR)$(mandir)/man1/ctrlproxy-admin.1 
	rm -f $(DESTDIR)$(HELPFILE)
	rm -f $(DESTDIR)$(mandir)/man5/ctrlproxy_config.5 
	rm -f $(DESTDIR)$(docdir)/user-guide.html
	-rmdir $(DESTDIR)$(mandir)/man1
	-rmdir $(DESTDIR)$(mandir)/man5
	-rmdir $(DESTDIR)$(docdir)
