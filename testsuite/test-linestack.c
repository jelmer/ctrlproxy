#include <stdio.h>
#include <string.h>
#include "ctrlproxy.h"

FILE *debugfd = NULL;

int main(int argc, char **argv)
{
	struct line *l;
	GSList *ll;
	struct linestack_context *c;
	char buf[4096];
	xmlNodePtr cur = xmlNewNode(NULL, "plugin");

	if(argc < 2) {
		fprintf(stderr, "Usage: %s plugin-path\n", argv[0]);
		return 1;
	}

	xmlSetProp(cur, "file", argv[1]);

	load_plugin(cur);

	c = linestack_new(NULL, argv[2]);
	
	while(!feof(stdin)) {
		fgets(buf, sizeof(buf), stdin);
		if(!strcmp(buf, "CLEAR\n")) linestack_clear(c);
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
	
	return 0;
}
