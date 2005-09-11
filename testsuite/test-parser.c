#include <stdio.h>
#include <malloc.h>
#include "ircdtorture.h"
#include "line.h"

static const char *malformed[] = {
	"PRIVMSG :foo :bar",
	":bar :bar PRIVMSG foo",
	"",
	": ",
	" ",
	NULL
};

static int parser_malformed(void)
{
	struct line *l;
	char *raw;
	int i;
	for (i = 0; malformed[i]; i++) {
		l = irc_parse_line(malformed[i]);
		if (!l) continue;
		raw = irc_line_string(l);
		free(raw);
		free_line(l);
	}

	return 0;
}

#define NUM_RUNS 200

static int parser_random(void)
{
	struct line *l;
	char *raw;
	char buf[4096];
	FILE *f = fopen("/dev/urandom", "r");
	int i;

	if (!f) {
		perror("Couldn't open /dev/urandom");
		return -1;
	}

	for (i = 0; i < 200; i++) {
		if (!fgets(buf, sizeof(buf)-2, f)) {
			perror("error reading random data");
			return -1;
		}
	
		l = irc_parse_line(buf);
		if (!l) continue;
		raw = irc_line_string(l);
		free(raw);
		free_line(l);
	}

	fclose(f);

	return 0;
}

void ircdtorture_init(void)
{
	register_test("PARSER-MALFORMED", parser_malformed);
	register_test("PARSER-RANDOM", parser_random);
}
