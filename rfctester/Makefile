-include ../Makefile.settings
CFLAGS += -I.. -DHAVE_CONFIG_H
CFLAGS+=-ansi -Wall -DMODULESDIR=\"$(modulesdir)\"

.PHONY: all clean distclean install 

all: ircdtorture $(patsubst %.c,%.so,$(wildcard test-*.c))

ircdtorture: ircdtorture.o ../line.o 
	$(CC) $(LIBS) -o $@ $^ 

$(patsubst %.c,%.so,$(wildcard test-*.c)): %.so: %.o

%.so: 
	$(CC) $(LIBS) $(CFLAGS) -shared -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $<

install: 

clean:
	rm -f *.o *.out *.tmp

distclean: clean
