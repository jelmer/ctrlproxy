all: libfoo.so

OBJS = foo.o

%.so: $(OBJS)
	$(CC) -o $@ -shared $(OBJS) `pkg-config --libs ctrlproxy`

%.o: %.c
	$(CC) -c -o $@ $< `pkg-config --cflags ctrlproxy`
