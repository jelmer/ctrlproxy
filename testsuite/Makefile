-include ../Makefile.settings
CFLAGS += -I.. -DHAVE_CONFIG_H
CFLAGS+=-ansi -Wall -DMODULESDIR=\"$(modulesdir)\"
TEST_SERVER := localhost
TEST_PORT := 6667

.PHONY: all clean distclean install ctrlproxyrc.torture

all: torture $(patsubst %.c,lib%.so,$(wildcard test-*.c))

torture: torture.o 
	$(CC) $(LIBS) -o $@ $^ 

$(patsubst %.c,lib%.so,$(wildcard test-*.c)): lib%.so: %.o
libtest-cmp.so: test-cmp.o ../util.o
libtest-state.so: test-state.o ../state.o ../util.o ../isupport.o ../line.o
libtest-parser.so: test-parser.o ../line.o

lib%.so: 
	$(CC) $(LIBS) $(CFLAGS) -shared -o $@ $^

ctrlproxyrc.torture: ctrlproxyrc.torture.in
	sed -e 's/@SERVER@/$(TEST_SERVER)/;s/@PORT@/$(TEST_PORT)/;' < $< > $@

test: torture $(patsubst %.c,lib%.so,$(wildcard test-*.c)) 
	./$< $(patsubst lib%.so,%,$(filter libtest-%.so,$^))

rfctest: ../rfctester/ircdtorture $(patsubst %.c,%.so,$(wildcard ../rfctester/test-*.c)) ctrlproxyrc.torture
	$< $(foreach I,$(filter test-%.so,$^),-m $(shell pwd)/../rfctester/$(I)) -- ../ctrlproxy -i TEST -r ctrlproxyrc.torture

%.o: %.c
	$(CC) $(CFLAGS) -c $<

install: 

clean:
	rm -f *.o *.out *.tmp *.so

distclean: clean
