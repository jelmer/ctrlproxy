/*
    ircdtorture: an IRC RFC compliancy tester
	(c) 2005 Jelmer Vernooij <jelmer@nl.linux.org>

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
#include <unistd.h>
#include <string.h>
#include <check.h>
#include "ctrlproxy.h"

gboolean network_nick_set_nick(struct network_nick *, const char *);
gboolean network_nick_set_hostmask(struct network_nick *, const char *);
struct network_nick *find_add_network_nick(struct irc_network_state *n, const char *name);

START_TEST(state_modes_set_mode)
	irc_modes_t in;
	modes_clear(in);
	fail_unless (modes_set_mode(in, '@'));
	fail_unless (in[(unsigned char)'@'] == TRUE);
	fail_unless (modes_set_mode(in, '%'));
	fail_unless (in[(unsigned char)'%'] == TRUE);
END_TEST

START_TEST(state_prefixes_remove_prefix)
	irc_modes_t in;
	memset(in, 0, sizeof(in));
	fail_if (modes_unset_mode(in, '@'));
	in[(unsigned char)'%'] = TRUE;
	fail_unless (modes_unset_mode(in, '%'));
	fail_unless (in[(unsigned char)'%'] == FALSE);
	fail_if (modes_unset_mode(in, '%'));
END_TEST

START_TEST(state_init)
	struct irc_network_state *ns = network_state_init("bla", "Gebruikersnaam", "Computernaam");

	fail_if(ns == NULL, "network_state_init returned NULL");

	fail_unless (strcmp(ns->me.nick, "bla") == 0);
	fail_unless (strcmp(ns->me.username, "Gebruikersnaam") == 0);
	fail_unless (strcmp(ns->me.hostname, "Computernaam") == 0);

	fail_unless (g_list_length(ns->channels) == 0);
END_TEST

void state_process(struct irc_network_state *ns, const char *line)
{
	struct irc_line *l;
	l = irc_parse_line(line);
	g_assert(l);
	g_assert(state_handle_data(ns, l));
	free_line(l);
}

START_TEST(state_join_me)
	struct irc_network_state *ns = network_state_init("bla", "Gebruikersnaam", "Computernaam");
	struct irc_channel_state *cs;

	fail_if (!ns);

	state_process(ns, ":bla!user@host JOIN #examplechannel");

	fail_unless (g_list_length(ns->channels) == 1);
	
	cs = ns->channels->data;

	fail_unless (strcmp(cs->name, "#examplechannel") == 0);
END_TEST

START_TEST(state_join_other)
	struct irc_network_state *ns = network_state_init("bla", "Gebruikersnaam", "Computernaam");
	struct irc_channel_state *cs;

	fail_if (!ns);

	state_process(ns, ":bla!user@host JOIN #examplechannel");
	state_process(ns, ":foo!userx@host JOIN #examplechannel");

	fail_unless (g_list_length(ns->channels) == 1);
	
	cs = ns->channels->data;
	fail_unless (g_list_length(cs->nicks) == 2);
END_TEST


START_TEST(state_topic)
	struct irc_network_state *ns = network_state_init("bla", "Gebruikersnaam", "Computernaam");
	struct irc_channel_state *cs;

	fail_if (!ns);

	state_process(ns, ":bla!user@host JOIN #examplechannel");
	state_process(ns, ":bla!user@host TOPIC #examplechannel :This is the topic");

	cs = ns->channels->data;

	fail_unless (strcmp(cs->topic, "This is the topic") == 0);
	fail_unless (abs(cs->topic_set_time - time(NULL)) < 5);
	fail_unless (strcmp(cs->topic_set_by, "bla") == 0);
END_TEST


START_TEST(state_part)
	struct irc_network_state *ns = network_state_init("bla", "Gebruikersnaam", "Computernaam");
	struct irc_channel_state *cs;

	fail_if (!ns);

	state_process(ns, ":bla!user@host JOIN #examplechannel");

	fail_unless (g_list_length(ns->channels) == 1);
	
	cs = ns->channels->data;

	fail_unless (strcmp(cs->name, "#examplechannel") == 0);
	
	state_process(ns, ":bla!user@host PART #examplechannel");

	fail_unless (g_list_length(ns->channels) == 0);
END_TEST

START_TEST(state_cycle)
	struct irc_network_state *ns = network_state_init("bla", "Gebruikersnaam", "Computernaam");
	struct irc_channel_state *cs;

	fail_if (!ns);

	state_process(ns, ":bla!user@host JOIN #examplechannel");

	fail_unless (g_list_length(ns->channels) == 1);
	
	cs = ns->channels->data;

	fail_unless (strcmp(cs->name, "#examplechannel") == 0);
	
	state_process(ns, ":bla!user@host PART #examplechannel");

	fail_unless (g_list_length(ns->channels) == 0);

	state_process(ns, ":bla!user@host JOIN #examplechannel");

	fail_unless (g_list_length(ns->channels) == 1);
END_TEST

START_TEST(state_kick)
	struct irc_network_state *ns = network_state_init("bla", "Gebruikersnaam", "Computernaam");
	struct irc_channel_state *cs;

	fail_if (!ns);

	state_process(ns, ":bla!user@host JOIN #examplechannel");

	fail_unless (g_list_length(ns->channels) == 1);
	
	cs = ns->channels->data;

	fail_unless (strcmp(cs->name, "#examplechannel") == 0);
	
	state_process(ns, ":bloe!anotheruser@host KICK #examplechannel bla :Doei");

	fail_unless (g_list_length(ns->channels) == 0);
END_TEST

START_TEST(state_nick_change_my)
	struct irc_network_state *ns = network_state_init("bla", "Gebruikersnaam", "Computernaam");

	fail_if(ns == NULL);

	state_process(ns, ":bla!user@host NICK blie");

	fail_unless(strcmp(ns->me.nick, "blie") == 0);
END_TEST

START_TEST(state_nick_change_other)
	struct irc_network_state *ns = network_state_init("bla", "Gebruikersnaam", "Computernaam");

	fail_if(ns == NULL);

	state_process(ns, ":foo!user@bar PRIVMSG bla :Hi");

	state_process(ns, ":foo!user@bar NICK blie");

	fail_if (find_network_nick(ns, "foo") != NULL);
	fail_if (find_network_nick(ns, "blie") == NULL);
END_TEST

START_TEST(state_set_nick)
	struct network_nick nn;
	memset(&nn, 0, sizeof(nn));
	
	fail_if (!network_nick_set_nick(&nn, "mynick"));
	fail_unless (strcmp(nn.nick, "mynick") == 0);
	fail_unless (strcmp(nn.hostmask, "mynick!(null)@(null)") == 0);
	fail_if (!network_nick_set_nick(&nn, "mynick"));
	fail_unless (strcmp(nn.nick, "mynick") == 0);
	fail_unless (strcmp(nn.hostmask, "mynick!(null)@(null)") == 0);
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

START_TEST(state_find_network_nick)
	struct irc_network_state *ns = network_state_init("bla", "Gebruikersnaam", "Computernaam");

	fail_if(ns == NULL);

	state_process(ns, ":foo!user@host PRIVMSG bla :Hoi");

	fail_unless(find_network_nick(ns, "foo") != NULL);
	fail_unless(find_network_nick(ns, "foobla") == NULL);
END_TEST

START_TEST(state_find_add_network_nick)
	struct irc_network_state *ns = network_state_init("bla", "Gebruikersnaam", "Computernaam");

	fail_if(ns == NULL);

	fail_if (find_add_network_nick(ns, "foo") == NULL);
	fail_if (find_add_network_nick(ns, "foo") == NULL);
END_TEST

START_TEST(state_handle_state_data)
	struct irc_network_state *ns = network_state_init("bla", "Gebruikersnaam", "Computernaam");
	struct irc_line l;
	char *args1[] = {"JOIN", "#bla", NULL};
	char *args2[] = {"UNKNOWN", "#bla", NULL};

	memset(&l, 0, sizeof(l));
	l.origin = "foo";

	fail_if(ns == NULL);

	fail_unless (state_handle_data(ns, NULL) == FALSE);
	fail_unless (state_handle_data(ns, &l) == FALSE);
	l.argc = 2;
	l.args = args1;
	fail_unless (state_handle_data(ns, &l) == TRUE);
	l.argc = 2;
	l.args = args2;
	fail_unless (state_handle_data(ns, &l) == FALSE);
END_TEST

START_TEST(test_string2mode)
	irc_modes_t modes;
	modes_clear(modes);
	string2mode("+", modes);
	string2mode("+o-o", modes);
	fail_unless(modes['o'] == FALSE);
	string2mode("+o", modes);
	fail_unless(modes['o'] == TRUE);
	string2mode("+oa", modes);
	fail_unless(modes['a'] == TRUE);
	fail_unless(modes['o'] == TRUE);
	string2mode("+o-a", modes);
	fail_unless(modes['a'] == FALSE);
	fail_unless(modes['o'] == TRUE);
END_TEST

START_TEST(test_mode2string)
	char *ret;
	irc_modes_t modes;
	modes_clear(modes);
	fail_unless(mode2string(modes) == NULL);
	modes['o'] = TRUE;
	ret = mode2string(modes);
	fail_unless(strcmp(ret, "+o") == 0);
	modes['k'] = TRUE;
	ret = mode2string(modes);
	fail_unless(strcmp(ret, "+ko") == 0);
END_TEST

Suite *state_suite(void)
{
	Suite *s = suite_create("state");
	TCase *tc_core = tcase_create("Core");
	suite_add_tcase(s, tc_core);
	tcase_add_test(tc_core, state_init);
	tcase_add_test(tc_core, state_join_me);
	tcase_add_test(tc_core, state_join_other);
	tcase_add_test(tc_core, state_topic);
	tcase_add_test(tc_core, state_part);
	tcase_add_test(tc_core, state_cycle);
	tcase_add_test(tc_core, state_kick);
	tcase_add_test(tc_core, state_set_nick);
	tcase_add_test(tc_core, state_set_nick);
	tcase_add_test(tc_core, state_set_hostmask);
	tcase_add_test(tc_core, state_nick_change_my);
	tcase_add_test(tc_core, state_nick_change_other);
	tcase_add_test(tc_core, state_find_network_nick);
	tcase_add_test(tc_core, state_find_add_network_nick);
	tcase_add_test(tc_core, state_handle_state_data);
	tcase_add_test(tc_core, state_modes_set_mode);
	tcase_add_test(tc_core, state_prefixes_remove_prefix);
	tcase_add_test(tc_core, test_mode2string);
	tcase_add_test(tc_core, test_string2mode);
	return s;
}
