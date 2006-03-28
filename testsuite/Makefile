-include ../Makefile.settings

GCOV_CFLAGS = -ftest-coverage -fprofile-arcs
GCOV_LIBS = -lgcov

ifeq ($(WITH_GCOV),1)
LIBS += $(GCOV_LIBS) $(GCOV_CFLAGS)
endif

CFLAGS += -I.. -DHAVE_CONFIG_H
CFLAGS +=-ansi -Wall -DMODULESDIR=\"$(modulesdir)\"
TEST_SERVER := localhost
TEST_PORT := 6667

.PHONY: all clean distclean install ctrlproxyrc.torture

all: torture $(patsubst %.c,lib%.so,$(wildcard test-*.c))

torture: torture.o 
	$(CC) $(LIBS) -o $@ $^ 

$(patsubst %.c,lib%.so,$(wildcard test-*.c)): lib%.so: %.o
libtest-cmp.so: test-cmp.o ../util.o
libtest-state.so: test-state.o ../state.o ../util.o ../isupport.o ../line.o ../log.o
libtest-isupport.so: test-isupport.o ../state.o ../util.o ../isupport.o ../line.o ../log.o
libtest-parser.so: test-parser.o ../line.o
libtest-client.so: test-client.o ../client.o ../line.o ../hooks.o ../log.o ../cache.o ../redirect.o ../network.o ../posix.o ../util.o ../state.o ../linestack.o ../gen_config.o ../settings.o ../isupport.o ../plugins.o

lib%.so: 
	$(CC) $(LIBS) $(CFLAGS) -shared -o $@ $^

ctrlproxyrc.torture: ctrlproxyrc.torture.in
	sed -e 's/@SERVER@/$(TEST_SERVER)/;s/@PORT@/$(TEST_PORT)/;' < $< > $@

test: torture $(patsubst %.c,lib%.so,$(wildcard test-*.c)) 
	$(VALGRIND) ./$< $(patsubst lib%.so,%,$(filter libtest-%.so,$^))

../rfctester/%:
	$(MAKE) -C ../rfctester $*

rfctest: ../rfctester/ircdtorture $(patsubst %.c,%.so,$(wildcard ../rfctester/test-*.c)) ctrlproxyrc.torture
	$< $(foreach I,$(filter ../rfctester/test-%.so,$^),-m $(shell pwd)/../rfctester/$(I)) -- ../ctrlproxy -d 0 -i TEST -r ctrlproxyrc.torture

%.o: %.c
	@echo Compiling $<
	@$(CC) $(CFLAGS) -c $<

install: 

clean:
	rm -f *.o *.out *.tmp *.so

distclean: clean
