MODS = log.so client_simple.so

all: $(MODS)

%.so: %.c
	$(CC) -I.. -Wall -g3 -shared -o $@ $<

clean:
	rm -f $(MODS)
