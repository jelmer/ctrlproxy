#include <stdio.h>
#include "ctrlproxy.h"

FILE *debugfd = NULL;

static filter_function ff = NULL;

void add_filter(char *name, filter_function f) 
{
	ff = f;
}

gboolean add_filter_ex(char *name, filter_function f, char *classname, int priority) {
	ff = f;
	return TRUE;
}

int main(int argc, char **argv)
{
	struct line *l;
	char buf[4096];
	char *raw;
	xmlNodePtr cur = xmlNewNode(NULL, "plugin");

	if(argc < 2) {
		fprintf(stderr, "Usage: %s plugin-path\n", argv[0]);
		return 1;
	}
	
	xmlSetProp(cur, "file", argv[1]);

	load_plugin(cur);

	if(!ff) {
		fprintf(stderr, "Module doesn't provide filter\n");
		return 1;
	}
	
	while(!feof(stdin)) {
		fgets(buf, sizeof(buf), stdin);
		l = irc_parse_line(buf);
		if(l) {
			raw = irc_line_string(l);
			ff(l);
			free(raw);
			free_line(l);
		} else { 
			fprintf(stderr, "Unparseable\n");
		}
	}
	return 0;
}
