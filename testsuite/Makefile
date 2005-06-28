-include ../Makefile.settings
CFLAGS += -I.. -DHAVE_CONFIG_H
CFLAGS+=-ansi -Wall -DMODULESDIR=\"$(modulesdir)\"

.PHONY: all clean distclean install

all: ircdtorture

ircdtorture: ircdtorture.o ../line.o 
	$(CC) $(LIBS) -o $@ $^ 

$(patsubst %.c,%.so,$(wildcard test-*.c)): %.so: %.o
test-cmp.so: test-cmp.o ../util.o

%.so: 
	$(CC) $(CFLAGS) -shared -o $@ $^

torture: ircdtorture $(patsubst %.c,%.so,$(wildcard test-*.c))
	./ircdtorture $(foreach I,$(filter-out $<,$^),-m $(shell pwd)/$(I)) -- ../ctrlproxy -i Freenode -r ../ctrlproxyrc.default

%.o: %.c
	$(CC) $(CFLAGS) -c $<

install: 

clean:
	rm -f *.o *.out *.tmp

distclean: clean
