#include <stdio.h>
#include "line.h"

FILE *debugfd = NULL;

int main(int argc, char **argv)
{
	struct line *l;
	char buf[4096];
	char *raw;
	
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
