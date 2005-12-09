-include ../Makefile.settings
CFLAGS += -I.. -DHAVE_CONFIG_H
CFLAGS+=-ansi -Wall -DMODULESDIR=\"$(modulesdir)\"

.PHONY: all clean distclean install 

ifeq ($(WITH_GCOV),1)
LIBS += -lgcov -ftest-coverage -fprofile-arcs
endif

all: ircdtorture 

ircdtorture: ircdtorture.o ../line.o $(patsubst %.c,%.o,$(wildcard test-*.c)) 
	$(CC) $(LIBS) -o $@ $^ 

%.o: %.c
	$(CC) $(CFLAGS) -c $<

install: 

clean:
	rm -f *.o *.out *.tmp

distclean: clean
