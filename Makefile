MODS = log.so

all: ctrlproxy $(MODS)

OBJS = server.o util.o main.o modules.o

ctrlproxy: $(OBJS)
	$(CC) -ldl -rdynamic -o ctrlproxy $(OBJS)

%.o: %.c
	$(CC) -c $<

%.so: %.c
	$(CC) -shared -o $@ $<

clean:
	rm -f *.o ctrlproxy $(MODS)
