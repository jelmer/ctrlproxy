CC = gcc
prefix = /usr/local
exec_prefix = ${prefix}
INSTALL = /usr/bin/install -c
CFLAGS = -I.. -g -O2 -I/usr/local/include/glib-2.0 -I/usr/local/lib/glib-2.0/include -I/usr/include/libxml2  
moddir = ${exec_prefix}/lib/ctrlproxy

MODS = liblog_irssi.so libctcp.so liblinestack_memory.so libauto-away.so libstrip.so libadmin.so libsocket.so libnoticelog.so libnickserv.so liblog_custom.so libantiflood.so librepl_simple.so librepl_command.so librepl_highlight.so liblinestack_file.so libstats.so

all: $(MODS)

install: all
	$(INSTALL) -d $(DESTDIR)$(moddir)
	$(INSTALL) $(MODS) $(DESTDIR)$(moddir)

%.o: %.c
	$(CC) $(CFLAGS) -fPIC -c $<

libstats.so: stats.o
	$(CC) $(CFLAGS) -shared -o $@ $< -ltdb -lpcre

libsocket.so: socket.o network-openssl.o
	$(CC) $(CFLAGS) -shared -o $@ socket.o network-openssl.o -lssl

lib%.so: %.c
	$(CC) $(CFLAGS) -shared -o $@ $< 

clean:
	rm -f $(MODS) *.o

distclean: clean
	rm -f Makefile
