-include ../Makefile.settings
CFLAGS += -I.. -DHAVE_CONFIG_H
CFLAGS+=-ansi -Wall -DMODULESDIR=\"$(modulesdir)\"

.PHONY: all clean distclean install

all: ircdtorture test-linestack test-parser

ircdtorture: ircdtorture.o
	$(CC) $(LIBS) -o $@ $^

test-linestack: test-linestack.o ../line.o ../linestack.o ../plugins.o ../config.o ../util.o ../server.o ../state.o ../isupport.c ../hooks.o ../client.o ../posix.o
	$(CC) $(LIBS) -o $@ $^

test-parser: test-parser.o ../line.o
	$(CC) $(LIBS) -o $@ $^


%.o: %.c
	$(CC) $(CFLAGS) -c $<

install: 

clean:
	rm -f *.o test-parser *.out *.tmp

distclean: clean
	rm -f Makefile 

tests: test1 test2 test3

test1: test-parser
	@# Check to see whether the parser can handle all kinds of nifty 
	@# input without crashing
	@echo -n Test1: 
	@dd if=/dev/urandom of=random.in count=300 2>/dev/null > /dev/null
	@./test-parser < random.in 2>/dev/null > /dev/null
	@echo OK
	
test2: test-parser
	@# Check for some other data that might crash the parser
	@echo -n Test2: 
	@./test-parser < test1.data 2>/dev/null > test1.data.out
	@echo OK

TEST3_MODULES = ctcp log_custom log_irssi repl_command repl_highlight repl_lastdisconnect repl_limited repl_simple report_time debug auto-away antiflood admin

LINESTACK_MODULES = linestack_file linestack_memory

test-linestack_%: ../mods/liblinestack_%.so test-linestack
	@echo -n Test 4-$(patsubst test-linestack_%,%:,%@)
	@./test-linestack `pwd`/$< < test4.data > $@.tmp
	@cmp $@.tmp test4.data
	@echo OK

test4: test-linestack $(patsubst %,test-%,$(LINESTACK_MODULES))
