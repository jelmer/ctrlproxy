/*
    ircdtorture: an IRC RFC compliancy tester
	(c) 2005 Jelmer Vernooij <jelmer@nl.linux.org>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
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
#include <unistd.h>
#include <string.h>
#include <check.h>
#include "ctrlproxy.h"

gboolean network_nick_set_nick(struct network_nick *, const char *);
gboolean network_nick_set_hostmask(struct network_nick *, const char *);

START_TEST(state_init)
	struct network_state *ns = network_state_init(NULL, "bla", "Gebruikersnaam", "Computernaam");

	fail_if(ns == NULL, "network_state_init returned NULL");

	fail_unless (strcmp(ns->me.nick, "bla") == 0);
	fail_unless (strcmp(ns->me.username, "Gebruikersnaam") == 0);
	fail_unless (strcmp(ns->me.hostname, "Computernaam") == 0);

	fail_unless (g_list_length(ns->channels) == 0);
END_TEST

static void state_process(struct network_state *ns, const char *line)
{
	struct line *l;
	l = irc_parse_line(line);
	g_assert(l);
	g_assert(state_handle_data(ns, l));
	free_line(l);
}

START_TEST(state_join)
	struct network_state *ns = network_state_init(NULL, "bla", "Gebruikersnaam", "Computernaam");
	struct channel_state *cs;

	fail_if (!ns);

	state_process(ns, ":bla!user@host JOIN #examplechannel");

	fail_unless (g_list_length(ns->channels) == 1);
	
	cs = ns->channels->data;

	fail_unless (strcmp(cs->name, "#examplechannel") == 0);
END_TEST

START_TEST(state_part)
	struct network_state *ns = network_state_init(NULL, "bla", "Gebruikersnaam", "Computernaam");
	struct channel_state *cs;

	fail_if (!ns);

	state_process(ns, ":bla!user@host JOIN #examplechannel");

	fail_unless (g_list_length(ns->channels) == 1);
	
	cs = ns->channels->data;

	fail_unless (strcmp(cs->name, "#examplechannel") == 0);
	
	state_process(ns, ":bla!user@host PART #examplechannel");

	fail_unless (g_list_length(ns->channels) == 0);
END_TEST

START_TEST(state_nick_change)
	struct network_state *ns = network_state_init(NULL, "bla", "Gebruikersnaam", "Computernaam");

	fail_if(ns == NULL);

	state_process(ns, ":bla!user@host NICK blie");

	fail_unless(strcmp(ns->me.nick, "blie") == 0);
END_TEST

START_TEST(state_set_nick)
	struct network_nick nn;
	memset(&nn, 0, sizeof(nn));
	
	fail_if (!network_nick_set_nick(&nn, "mynick"));
	fail_unless (strcmp(nn.nick, "mynick") == 0);
	fail_unless (strcmp(nn.hostmask, "mynick!~(null)@(null)") == 0);
	fail_if (!network_nick_set_nick(&nn, "mynick"));
	fail_unless (strcmp(nn.nick, "mynick") == 0);
	fail_unless (strcmp(nn.hostmask, "mynick!~(null)@(null)") == 0);
	fail_if (network_nick_set_nick(NULL, NULL));
END_TEST

START_TEST(state_set_hostmask)
	struct network_nick nn;
	memset(&nn, 0, sizeof(nn));

	fail_if (!network_nick_set_hostmask(&nn, "ikke!~uname@uhost"));
	fail_if (!nn.nick || strcmp(nn.nick, "ikke") != 0);
	fail_if (!nn.username || strcmp(nn.username, "~uname") != 0);
	fail_if (!nn.hostname || strcmp(nn.hostname, "uhost") != 0);
	fail_if (!network_nick_set_hostmask(&nn, "ikke!~uname@uhost"));
	fail_if (network_nick_set_hostmask(NULL, NULL));
	fail_if (network_nick_set_hostmask(&nn, NULL));

	fail_if (network_nick_set_hostmask(&nn, "ikkeongeldig"));
	fail_if (network_nick_set_hostmask(&nn, "ikke!ongeldig"));
END_TEST

START_TEST(state_marshall_simple)
	struct network_state *s, *t;
	size_t len1, len2;
	char *data1, *data2;

	s = network_state_init(NULL, "nick", "uname", "uhost");
	data1 = network_state_encode(s, &len1);
	t = network_state_decode(data1, len1, NULL);
	data2 = network_state_encode(s, &len2);

	fail_unless (len1 == len2);

	fail_unless (memcmp(data1, data2, len1) == 0);

	fail_unless (strcmp(s->me.nick, t->me.nick) == 0);
	fail_unless (strcmp(s->me.username, t->me.username) == 0);
	fail_unless (strcmp(s->me.hostname, t->me.hostname) == 0);

	free_network_state(t);
END_TEST

gboolean init_log(const char *lf);

Suite *state_suite(void)
{
	Suite *s = suite_create("state");
	TCase *tc_core = tcase_create("Core");
	suite_add_tcase(s, tc_core);
	tcase_add_test(tc_core, state_init);
	/* FIXME: Setup */
	init_log("test-state");
	tcase_add_test(tc_core, state_join);
	tcase_add_test(tc_core, state_part);
	tcase_add_test(tc_core, state_set_nick);
	tcase_add_test(tc_core, state_set_hostmask);
	tcase_add_test(tc_core, state_nick_change);
	tcase_add_test(tc_core, state_marshall_simple);
	return s;
}
