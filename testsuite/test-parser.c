#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <check.h>
#include "line.h"
#include "torture.h"

static const char *malformed[] = {
	"PRIVMSG :foo :bar",
	":bar :bar PRIVMSG foo",
	"",
	": ",
	" ",
	NULL
};

START_TEST(parser_empty)
	struct line *l;
	
	l = irc_parse_line("");

	fail_unless(l->argc == 1);

	l = irc_parse_line("\r\n");

	fail_unless(l->argc == 1);
END_TEST

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

START_TEST(parser_recv_line)
	GIOChannel *ch1, *ch2;
	struct line *l;
	GIConv iconv;

	g_io_channel_pair(&ch1, &ch2);
	g_io_channel_set_flags(ch1, G_IO_FLAG_NONBLOCK, NULL);
	g_io_channel_set_flags(ch2, G_IO_FLAG_NONBLOCK, NULL);

	iconv = g_iconv_open("UTF-8", "UTF-8");

	g_io_channel_write_chars(ch2, "PRIVMSG :bla\r\nFOO", -1, NULL, NULL);
	g_io_channel_flush(ch2, NULL);

	fail_unless(irc_recv_line(ch1, iconv, NULL, &l) == G_IO_STATUS_NORMAL);
	fail_unless(l->argc == 2);
	fail_unless(!strcmp(l->args[0], "PRIVMSG"));
	fail_unless(!strcmp(l->args[1], "bla"));
	fail_unless(irc_recv_line(ch1, iconv, NULL, &l) == G_IO_STATUS_AGAIN);

	g_iconv_close(iconv);
END_TEST

START_TEST(parser_recv_line_invalid)
	GIOChannel *ch1, *ch2;
	struct line *l;
	GIConv iconv;

	g_io_channel_pair(&ch1, &ch2);
	g_io_channel_set_flags(ch1, G_IO_FLAG_NONBLOCK, NULL);
	g_io_channel_set_flags(ch2, G_IO_FLAG_NONBLOCK, NULL);
	g_io_channel_set_encoding(ch1, NULL, NULL);
	g_io_channel_set_encoding(ch2, NULL, NULL);

	iconv = g_iconv_open("ISO8859-1", "UTF-8");

	g_io_channel_write_chars(ch2, "PRIVMSG :bl\366a\r\n", -1, NULL, NULL);
	g_io_channel_flush(ch2, NULL);

	fail_unless(irc_recv_line(ch1, iconv, NULL, &l) == G_IO_STATUS_ERROR);
	fail_unless(l == NULL);
	fail_unless(irc_recv_line(ch1, iconv, NULL, &l) == G_IO_STATUS_AGAIN);

	g_iconv_close(iconv);
END_TEST



START_TEST(parser_recv_line_iso8859)
	GIOChannel *ch1, *ch2;
	struct line *l;
	GIConv iconv;

	g_io_channel_pair(&ch1, &ch2);
	g_io_channel_set_flags(ch1, G_IO_FLAG_NONBLOCK, NULL);
	g_io_channel_set_flags(ch2, G_IO_FLAG_NONBLOCK, NULL);
	g_io_channel_set_encoding(ch1, NULL, NULL);
	g_io_channel_set_encoding(ch2, NULL, NULL);

	iconv = g_iconv_open("UTF-8", "ISO8859-1");

	fail_if(iconv == (GIConv)-1);

	g_io_channel_write_chars(ch2, "PRIVMSG \366 p\r\n", -1, NULL, NULL);
	g_io_channel_flush(ch2, NULL);

	fail_unless(irc_recv_line(ch1, iconv, NULL, &l) == G_IO_STATUS_NORMAL);
	fail_unless(l->argc == 3);
	fail_unless(!strcmp(l->args[0], "PRIVMSG"));
	fail_unless(!strcmp(l->args[1], "ö"));
	fail_unless(irc_recv_line(ch1, iconv, NULL, &l) == G_IO_STATUS_AGAIN);

	g_iconv_close(iconv);
END_TEST



START_TEST(parser_dup)
	struct line l, *m;
	char *args[] = { "x", "y", "z", NULL };
	l.origin = "bla";
	l.argc = 3;
	l.args = args;
	l.has_endcolon = 1;

	m = linedup(&l);

	fail_if (strcmp(l.origin, m->origin));
	fail_if (l.argc != m->argc);
	fail_if (strcmp(l.args[0], m->args[0]));
	fail_if (strcmp(l.args[1], m->args[1]));
	fail_if (strcmp(l.args[2], m->args[2]));

	l.origin = NULL;
	m = linedup(&l);
	fail_if (m->origin);
END_TEST

START_TEST(send_args)
	GIOChannel *ch1, *ch2;
	char *str;

	g_io_channel_pair(&ch1, &ch2);
	g_io_channel_set_flags(ch1, G_IO_FLAG_NONBLOCK, NULL);
	g_io_channel_set_flags(ch2, G_IO_FLAG_NONBLOCK, NULL);
	g_io_channel_set_encoding(ch1, NULL, NULL);
	g_io_channel_set_encoding(ch2, NULL, NULL);

	irc_send_args(ch1, (GIConv)-1, NULL, "PRIVMSG", "foo", NULL);

	g_io_channel_read_line(ch2, &str, NULL, NULL, NULL);

	fail_unless(!strcmp(str, "PRIVMSG :foo\r\n"));
END_TEST

START_TEST(send_args_utf8)
	GIOChannel *ch1, *ch2;
	char *str;
	GIConv iconv;

	iconv = g_iconv_open("ISO8859-1", "UTF-8");

	g_io_channel_pair(&ch1, &ch2);
	g_io_channel_set_flags(ch1, G_IO_FLAG_NONBLOCK, NULL);
	g_io_channel_set_flags(ch2, G_IO_FLAG_NONBLOCK, NULL);
	g_io_channel_set_encoding(ch1, NULL, NULL);
	g_io_channel_set_encoding(ch2, NULL, NULL);

	irc_send_args(ch1, iconv, NULL, "PRIVMSG", "fooö", NULL);

	g_iconv_close(iconv);

	g_io_channel_read_line(ch2, &str, NULL, NULL, NULL);

	fail_unless(!strcmp(str, "PRIVMSG :foo\366\r\n"));
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
	tcase_add_test(tcase, parser_recv_line);
	tcase_add_test(tcase, parser_recv_line_iso8859);
	tcase_add_test(tcase, parser_recv_line_invalid);
	tcase_add_test(tcase, parser_empty);
	tcase_add_test(tcase, send_args);
	tcase_add_test(tcase, send_args_utf8);
	return s;
}
