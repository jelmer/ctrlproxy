#include <stdio.h>
#include "ctrlproxy.h"

FILE *debugfd = NULL;

void add_filter(char *name, filter_function f) 
{
	add_filter_Ex(name, f, NULL, 100);
}

gboolean add_filter_ex(char *name, filter_function, char *classname, int priority) {
	return FALSE;
}

int main(int argc, char **argv)
{
	struct line *l;
	char buf[4096];
	char *raw;

	/* FIXME: Load specified plugin */
	
	while(!feof(stdin)) {
		fgets(buf, sizeof(buf), stdin);
		l = irc_parse_line(buf);
		if(l) {
			raw = irc_line_string(l);
			puts(raw);
			free(raw);
			free_line(l);
		} else { 
			fprintf(stderr, "Unparseable\n");
		}
	}
	return 0;
}
