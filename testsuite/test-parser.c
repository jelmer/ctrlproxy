#include <stdio.h>
#include <malloc.h>
#include "ircdtorture.h"
#include "line.h"

char *malformed[] = {
	"PRIVMSG :foo :bar",
	":bar :bar PRIVMSG foo",
	"",
	": ",
	" ",
	NULL
};

int parser_malformed(void)
{
	struct line *l;
	char *raw;
	int i;
	for (i = 0; malformed[i]; i++) {
		l = irc_parse_line(malformed[i]);
		raw = irc_line_string(l);
		free(raw);
		free_line(l);
	}

	return 0;
}

#define NUM_RUNS 200

int parser_random(void)
{
	struct line *l;
	char *raw;
	char buf[4096];
	FILE *f = fopen("/dev/urandom", "r");
	int i;

	if (!f) {
		fprintf(stderr, "Couldn't open /dev/urandom");
		return -1;
	}

	for (i = 0; i < 200; i++) {
		fgets(buf, sizeof(buf)-1, f);
	
		l = irc_parse_line(buf);
		raw = irc_line_string(l);
		free(raw);
		free_line(l);
	}

	return 0;
}

void ircdtorture_init(void)
{
	register_test("PARSER-MALFORMED", parser_malformed);
	register_test("PARSER-RANDOM", parser_random);
}
