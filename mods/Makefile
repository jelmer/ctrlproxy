include ../Makefile.settings
MODS = log.so client_simple.so ctcp.so

all: $(MODS)

install: all
	mkdir -p $(MODDIR)
	cp $(MODS) $(MODDIR)

%.so: %.c
	$(CC) -I.. -Wall -g3 -shared -o $@ $<

clean:
	rm -f $(MODS)
