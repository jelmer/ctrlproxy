-include Makefile.settings
MODS = libnss.so

.PHONY: all install clean distclean

all: $(MODS)

install: all
	$(INSTALL) -d $(DESTDIR)$(modulesdir)
	$(INSTALL) $(MODS) $(DESTDIR)$(modulesdir)

%.o: %.c
	$(CC) $(CFLAGS) -Dinit_plugin=$*_init -Dfini_plugin=$*_fini -Dname_plugin=$*_name -Dload_config=$*_load_config -Dsave_config=$*_save_config -c $<

lib%.so: %.c
	$(CC) -fPIC $(CFLAGS) $(LDFLAGS) -shared -o $@ $<

clean:
	rm -f $(MODS) *.o

distclean: clean
	rm -f Makefile.settings
