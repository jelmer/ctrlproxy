#include <stdio.h>
#include <string.h>
#include "ctrlproxy.h"

FILE *debugfd = NULL;

int main(int argc, char **argv)
{
	struct line *l;
	GSList *ll;
	struct linestack_context *c;
	struct plugin *p;
	char buf[4096];

	if(argc < 2) {
		fprintf(stderr, "Usage: %s plugin-path\n", argv[0]);
		return 1;
	}

	p = new_plugin(argv[1]);
	load_plugin(p);

	c = linestack_new(NULL, argv[2]);
	
	while(!feof(stdin)) {
		fgets(buf, sizeof(buf), stdin);
		if(!strncmp(buf, "CLEAR", 5)) {
			linestack_clear(c);
			continue;
		}
		l = irc_parse_line(buf);
		if(l) {
			linestack_add_line(c, l);
			free_line(l);
		} else { 
			fprintf(stderr, "Unparseable\n");
		}
	}

	ll = linestack_get_linked_list(c);
	while(ll) {
		char *raw;
		struct line *rl = (struct line *)ll->data;
		raw = irc_line_string(rl);
		puts(raw);
		free(raw);
		ll = ll->next;
	}

	linestack_destroy(c);

	unload_plugin(p);
	
	return 0;
}
