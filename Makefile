all: ctrlproxy
	$(MAKE) -C mods all

OBJS = server.o util.o main.o modules.o conf.o

ctrlproxy: $(OBJS)
	$(CC) -lpopt -ldl -rdynamic -o ctrlproxy $(OBJS)

%.o: %.c
	$(CC) -g3 -Wall -c $<

clean:
	rm -f *.o ctrlproxy
	$(MAKE) -C mods clean
