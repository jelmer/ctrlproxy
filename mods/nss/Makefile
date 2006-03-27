-include Makefile.settings
MODS = libnss.so

.PHONY: all install clean distclean

all: $(MODS)

install: all
	$(INSTALL) -d $(DESTDIR)$(modulesdir)
	$(INSTALL) $(MODS) $(DESTDIR)$(modulesdir)

lib%.so: %.c
	$(CC) -fPIC $(CFLAGS) $(LDFLAGS) -shared -o $@ $<

clean:
	rm -f $(MODS) *.o

distclean: clean
	rm -f Makefile.settings
