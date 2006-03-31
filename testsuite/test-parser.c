#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include "torture.h"
#include "../line.h"

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

static int parser_vargs(void)
{
	struct line *l = irc_parse_line_args( "FOO", "x", "y", NULL);

	if (!l) return -1;
	if (strcmp(l->origin, "FOO") != 0) return -2;
	if (l->argc != 2) return -3;
	if (strcmp(l->args[0], "x") != 0) return -4;
	if (strcmp(l->args[1], "y") != 0) return -5;

	return 0;
}

static int parser_stringnl(void)
{
	struct line l;
	char *ret;
	char *args[] = { "x", "y", "z", NULL };
	l.origin = "foobar";
	l.argc = 3;
	l.args = args;
	l.has_endcolon = WITHOUT_COLON;

	ret = irc_line_string_nl(&l);

	if (strcmp(ret, ":foobar x y z\r\n") != 0) return -1;

	return 0;
}

static int parser_get_nick(void)
{
	struct line l;
	char *nick = line_get_nick(&l);

	l.origin = "foobar";
	if (strcmp(nick, "foobar") != 0) { g_free(nick); return -3; }
	l.origin = "foobar!~username@userhost";
	if (strcmp(nick, "foobar") != 0) { g_free(nick); return -4; }
	g_free(nick);
	return 0;
}

static int parser_dup(void)
{
	struct line l, *m;
	char *args[] = { "x", "y", "z", NULL };
	l.is_private = 1;
	l.origin = "bla";
	l.argc = 3;
	l.args = args;
	l.has_endcolon = 1;

	m = linedup(&l);

	if (l.is_private != m->is_private) return -1;
	if (strcmp(l.origin, m->origin)) return -3;
	if (l.argc != m->argc) return -4;
	if (strcmp(l.args[0], m->args[0])) return -5;
	if (strcmp(l.args[1], m->args[1])) return -6;
	if (strcmp(l.args[2], m->args[2])) return -7;

	l.origin = NULL;
	m = linedup(&l);
	if (m->origin) return -8;

	return 0;
}

void torture_init(void)
{
	register_test("PARSER-VARGS", parser_vargs);
	register_test("PARSER-STRINGNL", parser_stringnl);
	register_test("PARSER-MALFORMED", parser_malformed);
	register_test("PARSER-RANDOM", parser_random);
	register_test("PARSER-GETNICK", parser_get_nick);
	register_test("PARSER-DUP", parser_dup);
}
