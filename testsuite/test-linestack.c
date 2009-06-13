/*
	(c) 2006 Jelmer Vernooij <jelmer@nl.linux.org>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <stdio.h>
#include <string.h>
#include <check.h>
#include "ctrlproxy.h"
#include "torture.h"
#include "internals.h"

void stack_process(struct linestack_context *ctx, struct irc_network_state *ns, const char *line)
{
	struct irc_line *l;
	l = irc_parse_line(line);
	g_assert(l);
	g_assert(state_handle_data(ns, l));
	g_assert(linestack_insert_line(ctx, l, FROM_SERVER, ns));
	free_line(l);
}

#define null_equal(a,b) { \
	if ((a) == NULL && (b) == NULL) \
		return TRUE; \
	if ((a) == NULL || (b) == NULL) \
	return FALSE; \
}

static gboolean list_equal(GList *list1, GList *list2, GEqualFunc eq)
{
	GList *gl1, *gl2;
	null_equal(list1, list2);

	for (gl1 = list1, gl2 = list2; gl1 && gl2; gl1 = gl1->next, gl2 = gl2->next) {
		if (!eq(gl1->data, gl2->data))
			return FALSE;
	}

	if (gl1 != NULL || gl2 != NULL)
		return FALSE;

	return TRUE;
}

static gboolean modes_equal(const irc_modes_t a, const irc_modes_t b)
{
	return modes_cmp(a, b) == 0;
}

static gboolean str_equal(const char *a, const char *b)
{
	null_equal(a, b);

	return g_str_equal(a, b);
}

static gboolean channel_nick_equal(const struct channel_nick *nick1, const struct channel_nick *nick2)
{
	null_equal(nick1, nick2);

	return modes_equal(nick1->modes, nick2->modes) &&
		   str_equal(nick1->global_nick->nick, nick2->global_nick->nick) &&
		   str_equal(nick1->channel->name, nick2->channel->name);
}

static gboolean banlist_entry_equal(const struct nicklist_entry *entry1, const struct nicklist_entry *entry2)
{
	null_equal(entry1, entry2);

	return str_equal(entry1->hostmask, entry2->hostmask) &&
		   str_equal(entry1->by, entry2->by) &&
		   entry1->time_set == entry2->time_set;
}

static gboolean channel_state_equal(const struct irc_channel_state *channel1, const struct irc_channel_state *channel2)
{
	int i;
	null_equal(channel1, channel2);
	
	for (i = 0; i < MAXMODES; i++) {
		if (!str_equal(channel1->chanmode_option[i], channel2->chanmode_option[i]))
			return FALSE;

		if (!list_equal(channel1->chanmode_nicklist[i], channel2->chanmode_nicklist[i], (GEqualFunc)banlist_entry_equal))
			return FALSE;
	}

	return str_equal(channel1->name, channel2->name) &&
		   str_equal(channel1->topic, channel2->topic) &&
		   channel1->mode == channel2->mode &&
		   !memcmp(channel1->modes, channel2->modes, 255) &&
		   channel1->namreply_started == channel2->namreply_started &&
		   channel1->invitelist_started == channel2->invitelist_started &&
		   channel1->exceptlist_started == channel2->exceptlist_started &&
		   channel1->banlist_started == channel2->banlist_started &&
		   list_equal(channel1->nicks, channel2->nicks, (GEqualFunc)channel_nick_equal);
}

static gboolean network_info_equal(const struct irc_network_info *info1, const struct irc_network_info *info2)
{
	null_equal(info1, info2);

	return str_equal(info1->name, info2->name) &&
		   str_equal(info1->server, info2->server) &&
		   str_equal(info1->supported_user_modes, info2->supported_user_modes) &&
		   str_equal(info1->supported_channel_modes, info2->supported_channel_modes) &&
		   str_equal(info1->prefix, info2->prefix) &&
		   str_equal(info1->chantypes, info2->chantypes) &&
		   str_equal(info1->charset, info2->charset) &&
		   ((info1->chanmodes == NULL && info2->chanmodes == NULL) ||
		   (str_equal(info1->chanmodes[0], info2->chanmodes[0]) &&
		   str_equal(info1->chanmodes[1], info2->chanmodes[1]) &&
		   str_equal(info1->chanmodes[2], info2->chanmodes[2]) &&
		   str_equal(info1->chanmodes[3], info2->chanmodes[3]))) &&
		   info1->keylen == info2->keylen &&
		   info1->silence == info2->silence &&
		   info1->channellen == info2->channellen &&
		   info1->awaylen == info2->awaylen &&
		   info1->maxtargets == info2->maxtargets &&
		   info1->nicklen == info2->nicklen &&
		   info1->userlen == info2->userlen &&
		   info1->hostlen == info2->hostlen &&
		   info1->maxchannels == info2->maxchannels &&
		   info1->topiclen == info2->topiclen &&
		   info1->maxbans == info2->maxbans &&
		   info1->maxmodes == info2->maxmodes &&
		   info1->wallchops == info2->wallchops &&
		   info1->wallvoices == info2->wallvoices &&
		   info1->rfc2812 == info2->rfc2812 &&
		   info1->penalty == info2->penalty &&
		   info1->forced_nick_changes == info2->forced_nick_changes &&
		   info1->safelist == info2->safelist &&
		   info1->userip == info2->userip &&
		   info1->cprivmsg == info2->cprivmsg &&
		   info1->cnotice == info2->cnotice &&
		   info1->knock == info2->knock &&
		   info1->vchannels == info2->vchannels &&
		   info1->whox == info2->whox &&
		   info1->callerid == info2->callerid &&
		   info1->accept == info2->accept &&
		   info1->capab == info2->capab &&
		   info1->casemapping == info2->casemapping;
}

static gboolean network_nick_equal(const struct network_nick *nick1, const struct network_nick *nick2)
{
	null_equal(nick1, nick2);

	return nick1->query == nick2->query &&
		   str_equal(nick1->nick, nick2->nick) &&
		   str_equal(nick1->fullname, nick2->fullname) &&
		   str_equal(nick1->username, nick2->username) &&
		   str_equal(nick1->hostname, nick2->hostname) &&
		   !memcmp(nick1->modes, nick2->modes, 255) &&
		   list_equal(nick1->channel_nicks, nick2->channel_nicks, 
					  (GEqualFunc)channel_nick_equal);
}

static gboolean network_state_equal(const struct irc_network_state *state1, 
									const struct irc_network_state *state2)
{
	null_equal(state1, state2);

	return network_nick_equal(&state1->me, &state2->me) &&
		   network_info_equal(state1->info, state2->info) &&
		   list_equal(state1->channels, state2->channels, 
					  (GEqualFunc)channel_state_equal) &&
		   list_equal(state1->nicks, state2->nicks, 
					  (GEqualFunc)network_nick_equal);
}

const char *get_linestack_tempdir(const char *base)
{
	return g_build_filename("/tmp", base, NULL);
}

START_TEST(test_empty)
	struct irc_network_state *ns1, *ns2;
	struct linestack_context *ctx;
	
	ns1 = network_state_init("bla", "Gebruikersnaam", "Computernaam");
	ctx = create_linestack("test", TRUE, 
						   get_linestack_tempdir("test_empty"), ns1);

	ns2 = linestack_get_state(ctx, NULL);

	fail_unless (network_state_equal(ns1, ns2), 
				 "Network state returned not equal");
END_TEST

START_TEST(test_msg)
	struct irc_network_state *ns1;
	struct linestack_context *ctx;
	struct linestack_marker *lm;
	struct irc_client *cl;

	GIOChannel *ch1, *ch2;
	char *raw;
	
	ns1 = network_state_init("bla", "Gebruikersnaam", "Computernaam");
	ctx = create_linestack("test", TRUE, get_linestack_tempdir("msg"), ns1);

	lm = linestack_get_marker(ctx);

	stack_process(ctx, ns1, ":bla!Gebruikersnaam@Computernaam JOIN #bla");
	stack_process(ctx, ns1, ":bloe!Gebruikersnaam@Computernaam PRIVMSG #bla :hihi");

	g_io_channel_pair(&ch1, &ch2);
	g_io_channel_set_flags(ch1, G_IO_FLAG_NONBLOCK, NULL);
	g_io_channel_set_flags(ch2, G_IO_FLAG_NONBLOCK, NULL);
	cl = client_init_iochannel(NULL, ch1, "test");
	g_io_channel_unref(ch1);

	linestack_send(ctx, lm, NULL, cl, FALSE, FALSE, 0);
	client_disconnect(cl, "foo");

	g_io_channel_read_to_end(ch2, &raw, NULL, NULL);

	fail_unless(!strcmp(raw, ":bla!Gebruikersnaam@Computernaam JOIN #bla\r\n"
						     ":bloe!Gebruikersnaam@Computernaam PRIVMSG #bla :hihi\r\n"
							 "ERROR :foo\r\n"));
END_TEST

START_TEST(test_join_part)
	struct irc_network_state *ns1;
	struct linestack_context *ctx;
	struct linestack_marker *lm;
	struct irc_client *cl;

	GIOChannel *ch1, *ch2;
	char *raw;
	
	ns1 = network_state_init("bla", "Gebruikersnaam", "Computernaam");
	ctx = create_linestack("test", TRUE, get_linestack_tempdir("join_part"), ns1);

	lm = linestack_get_marker(ctx);

	stack_process(ctx, ns1, ":bla!Gebruikersnaam@Computernaam JOIN #bla");
	stack_process(ctx, ns1, ":bla!Gebruikersnaam@Computernaam PART #bla :hihi");

	g_io_channel_pair(&ch1, &ch2);
	g_io_channel_set_flags(ch1, G_IO_FLAG_NONBLOCK, NULL);
	g_io_channel_set_flags(ch2, G_IO_FLAG_NONBLOCK, NULL);
	cl = client_init_iochannel(NULL, ch1, "test");
	g_io_channel_unref(ch1);

	linestack_send(ctx, lm, NULL, cl, FALSE, FALSE, 0);
	client_disconnect(cl, "foo");

	g_io_channel_read_to_end(ch2, &raw, NULL, NULL);

	fail_unless(!strcmp(raw, ":bla!Gebruikersnaam@Computernaam JOIN #bla\r\n"
						     ":bla!Gebruikersnaam@Computernaam PART #bla :hihi\r\n"
							 "ERROR :foo\r\n"));
END_TEST



START_TEST(test_skip_msg)
	struct irc_network_state *ns1;
	struct linestack_context *ctx;
	struct linestack_marker *lm;
	struct irc_client *cl;

	GIOChannel *ch1, *ch2;
	char *raw;
	
	ns1 = network_state_init("bla", "Gebruikersnaam", "Computernaam");
	ctx = create_linestack("test", TRUE, get_linestack_tempdir("skip_msg"), ns1);

	stack_process(ctx, ns1, ":bloe!Gebruikersnaam@Computernaam PRIVMSG #bla :haha");

	lm = linestack_get_marker(ctx);

	stack_process(ctx, ns1, ":bla!Gebruikersnaam@Computernaam JOIN #bla");
	stack_process(ctx, ns1, ":bloe!Gebruikersnaam@Computernaam PRIVMSG #bla :hihi");

	g_io_channel_pair(&ch1, &ch2);
	g_io_channel_set_flags(ch1, G_IO_FLAG_NONBLOCK, NULL);
	g_io_channel_set_flags(ch2, G_IO_FLAG_NONBLOCK, NULL);
	cl = client_init_iochannel(NULL, ch1, "test");
	g_io_channel_unref(ch1);

	linestack_send(ctx, lm, NULL, cl, FALSE, FALSE, 0);
	client_disconnect(cl, "foo");

	g_io_channel_read_to_end(ch2, &raw, NULL, NULL);

	fail_unless(!strcmp(raw, ":bla!Gebruikersnaam@Computernaam JOIN #bla\r\n"
						     ":bloe!Gebruikersnaam@Computernaam PRIVMSG #bla :hihi\r\n"
							 "ERROR :foo\r\n"));
END_TEST

START_TEST(test_object_msg)
	struct irc_network_state *ns1;
	struct linestack_context *ctx;
	struct linestack_marker *lm;
	struct irc_client *cl;

	GIOChannel *ch1, *ch2;
	char *raw;
	
	ns1 = network_state_init("bla", "Gebruikersnaam", "Computernaam");
	ctx = create_linestack("test", TRUE, get_linestack_tempdir("get_object_msg"), ns1);

	lm = linestack_get_marker(ctx);

	stack_process(ctx, ns1, ":bla!Gebruikersnaam@Computernaam JOIN #foo");
	stack_process(ctx, ns1, ":bla!Gebruikersnaam@Computernaam JOIN #bla");
	stack_process(ctx, ns1, ":bloe!Gebruikersnaam@Computernaam PRIVMSG #foo :hihi");
	stack_process(ctx, ns1, ":bloe!Gebruikersnaam@Computernaam PRIVMSG #bar :hihi");
	stack_process(ctx, ns1, ":bloe!Gebruikersnaam@Computernaam PRIVMSG #blablie :hihi");
	stack_process(ctx, ns1, ":bloe!Gebruikersnaam@Computernaam PRIVMSG #bla :hihi");

	g_io_channel_pair(&ch1, &ch2);
	g_io_channel_set_flags(ch1, G_IO_FLAG_NONBLOCK, NULL);
	g_io_channel_set_flags(ch2, G_IO_FLAG_NONBLOCK, NULL);
	cl = client_init_iochannel(NULL, ch1, "test");
	g_io_channel_unref(ch1);

	linestack_send_object(ctx, "#bla", lm, NULL, cl, FALSE, FALSE, 0);
	client_disconnect(cl, "foo");

	g_io_channel_read_to_end(ch2, &raw, NULL, NULL);

	fail_unless(!strcmp(raw, ":bla!Gebruikersnaam@Computernaam JOIN #bla\r\n"
						     ":bloe!Gebruikersnaam@Computernaam PRIVMSG #bla :hihi\r\n"
							 "ERROR :foo\r\n"));
END_TEST


START_TEST(test_object_open)
	struct irc_network_state *ns1;
	struct linestack_context *ctx;
	struct linestack_marker *lm;
	struct irc_client *cl;

	GIOChannel *ch1, *ch2;
	char *raw;

	int j;
	
	ns1 = network_state_init("bla", "Gebruikersnaam", "Computernaam");
	ctx = create_linestack("test", TRUE, get_linestack_tempdir("test_object_open"), ns1);

	lm = linestack_get_marker(ctx);

	stack_process(ctx, ns1, ":bla!Gebruikersnaam@Computernaam JOIN #foo");
	stack_process(ctx, ns1, ":bla!Gebruikersnaam@Computernaam JOIN #bla");
	stack_process(ctx, ns1, ":bloe!Gebruikersnaam@Computernaam PRIVMSG #foo :hihi");
	stack_process(ctx, ns1, ":bloe!Gebruikersnaam@Computernaam PRIVMSG #bar :hihi");
	stack_process(ctx, ns1, ":bloe!Gebruikersnaam@Computernaam PRIVMSG #blablie :hihi");
	stack_process(ctx, ns1, ":bloe!Gebruikersnaam@Computernaam PRIVMSG #bla :hihi");

	for (j = 0; j < 4; j++) {
		g_io_channel_pair(&ch1, &ch2);
		g_io_channel_set_flags(ch1, G_IO_FLAG_NONBLOCK, NULL);
		g_io_channel_set_flags(ch2, G_IO_FLAG_NONBLOCK, NULL);
		cl = client_init_iochannel(NULL, ch1, "test");
		g_io_channel_unref(ch1);

		linestack_send_object(ctx, "#bla", NULL, NULL, cl, FALSE, FALSE, 0);
		client_disconnect(cl, "foo");

		g_io_channel_read_to_end(ch2, &raw, NULL, NULL);

		fail_unless(!strcmp(raw, ":bla!Gebruikersnaam@Computernaam JOIN #bla\r\n"
						":bloe!Gebruikersnaam@Computernaam PRIVMSG #bla :hihi\r\n"
						"ERROR :foo\r\n"));
	}
END_TEST

START_TEST(test_join)
	struct irc_network_state *ns1, *ns2;
	struct linestack_context *ctx;
	
	ns1 = network_state_init("bla", "Gebruikersnaam", "Computernaam");
	ctx = create_linestack("test", TRUE, get_linestack_tempdir("test_join"), ns1);

	stack_process(ctx, ns1, ":bla!Gebruikersnaam@Computernaam JOIN #bla");

	ns2 = linestack_get_state(ctx, linestack_get_marker(ctx));

	fail_unless (network_state_equal(ns1, ns2), "Network state returned not equal");
END_TEST

int seen = 0;

static gboolean line_track(struct irc_line *l, time_t t, void *data)
{
	fail_unless(!strcmp(l->args[0], "PRIVMSG"));
	fail_unless(seen == atoi(l->args[1]));
	seen++;
	return TRUE;
}

START_TEST(bench_lots_of_lines)
	struct irc_network_state *ns1;
	struct linestack_context *ctx;
	struct linestack_marker *marker;
	int i;

	ns1 = network_state_init("bla", "Gebruikersnaam", "Computernaam");
	ctx = create_linestack("test", TRUE, get_linestack_tempdir("base"), ns1);
	marker = linestack_get_marker(ctx);

	seen = 0;

	for (i = 0; i < 10000; i++) 
		linestack_insert_line(ctx, irc_parse_linef("PRIVMSG :%d", i), 
							  TO_SERVER, ns1);
	
	linestack_traverse(ctx, marker, NULL, line_track, stderr);
END_TEST

Suite *linestack_suite()
{
	Suite *s = suite_create("linestack");
	TCase *tc_core = tcase_create("core");
	suite_add_tcase(s, tc_core);
	tcase_add_test(tc_core, test_empty);
	tcase_add_test(tc_core, test_join);
	tcase_add_test(tc_core, test_msg);
	tcase_add_test(tc_core, test_skip_msg);
	tcase_add_test(tc_core, test_object_msg);
	tcase_add_test(tc_core, test_object_open);
	tcase_add_test(tc_core, test_join_part);
	tcase_add_test(tc_core, bench_lots_of_lines);
	return s;
}
