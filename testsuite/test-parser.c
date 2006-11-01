#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <check.h>
#include "line.h"

static const char *malformed[] = {
	"PRIVMSG :foo :bar",
	":bar :bar PRIVMSG foo",
	"",
	": ",
	" ",
	NULL
};

START_TEST(parser_malformed)
	struct line *l;
	char *raw;
	int j;

	for (j = 0; malformed[j]; j++) {
		l = irc_parse_line(malformed[j]);
		if (!l) continue;
		raw = irc_line_string(l);
		free(raw);
		free_line(l);
	}
END_TEST

#define NUM_RUNS 200

START_TEST(parser_random)
	struct line *l;
	char *raw;
	char buf[4096];
	FILE *f = fopen("/dev/urandom", "r");
	int j;

	fail_if (!f, "Couldn't open /dev/urandom");

	for (j = 0; j < 200; j++) {
		fail_if (!fgets(buf, sizeof(buf)-2, f), "error reading random data");
	
		l = irc_parse_line(buf);
		if (!l) continue;
		raw = irc_line_string(l);
		free(raw);
		free_line(l);
	}

	fclose(f);
END_TEST

START_TEST(parser_vargs)
	struct line *l = irc_parse_line_args( "FOO", "x", "y", NULL);

	fail_if (!l);
	fail_if (strcmp(l->origin, "FOO") != 0);
	fail_if (l->argc != 2);
	fail_if (strcmp(l->args[0], "x") != 0);
	fail_if (strcmp(l->args[1], "y") != 0);
END_TEST

START_TEST( parser_stringnl)
	struct line l;
	char *ret;
	char *args[] = { "x", "y", "z", NULL };
	l.origin = "foobar";
	l.argc = 3;
	l.args = args;
	l.has_endcolon = WITHOUT_COLON;

	ret = irc_line_string_nl(&l);

	fail_if (strcmp(ret, ":foobar x y z\r\n") != 0);
END_TEST

START_TEST(parser_get_nick)
	struct line l;
	char *nick;

	l.origin = "foobar";
	nick = line_get_nick(&l);
	fail_if (strcmp(nick, "foobar") != 0);
	l.origin = "foobar!~username@userhost";
	g_free(nick);
	nick = line_get_nick(&l);
	fail_if (strcmp(nick, "foobar") != 0);
	g_free(nick);
END_TEST

START_TEST(parser_dup)
	struct line l, *m;
	char *args[] = { "x", "y", "z", NULL };
	l.is_private = 1;
	l.origin = "bla";
	l.argc = 3;
	l.args = args;
	l.has_endcolon = 1;

	m = linedup(&l);

	fail_if (l.is_private != m->is_private);
	fail_if (strcmp(l.origin, m->origin));
	fail_if (l.argc != m->argc);
	fail_if (strcmp(l.args[0], m->args[0]));
	fail_if (strcmp(l.args[1], m->args[1]));
	fail_if (strcmp(l.args[2], m->args[2]));

	l.origin = NULL;
	m = linedup(&l);
	fail_if (m->origin);
END_TEST

Suite *parser_suite(void)
{
	Suite *s = suite_create("parser");
	TCase *tcase = tcase_create("core");
	suite_add_tcase(s, tcase);
	tcase_add_test(tcase, parser_vargs);
	tcase_add_test(tcase, parser_stringnl);
	tcase_add_test(tcase, parser_malformed);
	tcase_add_test(tcase, parser_random);
	tcase_add_test(tcase, parser_get_nick);
	tcase_add_test(tcase, parser_dup);
	return s;
}
