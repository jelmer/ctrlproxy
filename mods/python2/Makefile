-include Makefile.settings

.PHONY: all build install clean distclean

all: build libpython2.so

lib%.so: %.c
	$(CC) $(CFLAGS) $(PYCFLAGS) $(LDFLAGS) $(PYLDFLAGS) -shared -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) $(PYCFLAGS) -Dplugin=plugin_$* -c $<

build: ctrlproxy_wrap.c admin_wrap.c listener_wrap.c
	LDFLAGS="$(LDFLAGS)" CFLAGS="$(CFLAGS)" $(PYTHON) setup.py build

install: all
	$(PYTHON) setup.py install --root="$(DESTDIR)"
	$(INSTALL) libpython2.so $(DESTDIR)$(modulesdir)

clean:
	rm -f *_wrap.c *.pyc
	rm -f ctrlproxy.py admin.py listener.py
	$(PYTHON) setup.py clean
	rm -rf build/

distclean: clean
	rm -f Makefile.settings

%_wrap.c: ../%.i
	$(SWIG) -I.. -I../.. -python $*.i

%_wrap.c: ../../%.i
	$(SWIG) -I../.. -python $*.i
