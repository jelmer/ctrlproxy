include Makefile.settings

all: ctrlproxy
	$(MAKE) -C mods all

OBJS = server.o util.o main.o modules.o conf.o

ctrlproxy: $(OBJS)
	$(CC) -lpopt -ldl -rdynamic -o ctrlproxy $(OBJS)

%.o: %.c
	$(CC) -g3 -Wall -c $<

install: all
	cp ctrlproxy $(BINDIR)
	cp ctrlproxy.1 $(MANDIR)/man1

clean:
	rm -f *.o ctrlproxy
	$(MAKE) -C mods clean
