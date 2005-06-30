-include ../Makefile.settings
CFLAGS += -I.. -DHAVE_CONFIG_H
CFLAGS+=-ansi -Wall -DMODULESDIR=\"$(modulesdir)\"
TEST_SERVER := localhost
TEST_PORT := 6667

.PHONY: all clean distclean install ctrlproxyrc.torture

all: ircdtorture

ircdtorture: ircdtorture.o ../line.o 
	$(CC) $(LIBS) -o $@ $^ 

$(patsubst %.c,%.so,$(wildcard test-*.c)): %.so: %.o
test-cmp.so: test-cmp.o ../util.o
test-state.so: test-state.o ../state.o ../util.o ../isupport.o

%.so: 
	$(CC) $(LIBS) $(CFLAGS) -shared -o $@ $^

ctrlproxyrc.torture: ctrlproxyrc.torture.in
	sed -e 's/@SERVER@/$(TEST_SERVER)/;s/@PORT@/$(TEST_PORT)/;' < $< > $@

torture: ircdtorture $(patsubst %.c,%.so,$(wildcard test-*.c)) ctrlproxyrc.torture
	./ircdtorture $(foreach I,$(filter test-%.so,$^),-m $(shell pwd)/$(I)) -- ../ctrlproxy -i TEST -r ctrlproxyrc.torture

%.o: %.c
	$(CC) $(CFLAGS) -c $<

install: 

clean:
	rm -f *.o *.out *.tmp

distclean: clean
