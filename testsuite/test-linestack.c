/*
	(c) 2006 Jelmer Vernooij <jelmer@nl.linux.org>

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

#define null_equal(a,b) { \
	if ((a) == NULL && (b) == NULL) \
		return TRUE; \
	if ((a) == NULL || (b) == NULL) \
	return FALSE; \
}

struct hash_data {
	GEqualFunc fn;
	GHashTable *hash2;
	gboolean success;
};

static void hash_traverse_equal(void *key, void *val1, void *userdata)
{
	struct hash_data *hash = userdata;
	void *val2;

	val2 = g_hash_table_lookup(hash->hash2, key);

	hash->success &= hash->fn(val1, val2);
}

static gboolean hash_equal(GHashTable *hash1, GHashTable *hash2, GEqualFunc eqval)
{
	struct hash_data userdata;

	null_equal(hash1, hash2);
	
	/* Traverse over all keys in hash1 and make sure they exist
	 * with the same value in hash1 and hash2 */

	userdata.fn = eqval;
	userdata.hash2 = hash2;
	userdata.success = TRUE;
	g_hash_table_foreach(hash1, hash_traverse_equal, &userdata);
	if (!userdata.success)
		return FALSE;
	
	/* Traverse over all keys in hash2 and make sure 
	 * they exist in hash1 */

	userdata.fn = eqval;
	userdata.hash2 = hash1;
	g_hash_table_foreach(hash2, hash_traverse_equal, &userdata);

	return userdata.success;
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

static gboolean str_equal(const char *a, const char *b)
{
	null_equal(a, b);

	return g_str_equal(a, b);
}

static gboolean channel_nick_equal(const struct channel_nick *nick1, const struct channel_nick *nick2)
{
	null_equal(nick1, nick2);

	return nick1->mode == nick2->mode &&
		   str_equal(nick1->global_nick->nick, nick2->global_nick->nick) &&
		   str_equal(nick1->channel->name, nick2->channel->name);
}

static gboolean banlist_entry_equal(const struct banlist_entry *entry1, const struct banlist_entry *entry2)
{
	null_equal(entry1, entry2);

	return str_equal(entry1->hostmask, entry2->hostmask) &&
		   str_equal(entry1->by, entry2->by) &&
		   entry1->time_set == entry2->time_set;
}

static gboolean channel_state_equal(const struct channel_state *channel1, const struct channel_state *channel2)
{
	null_equal(channel1, channel2);

	return str_equal(channel1->name, channel2->name) &&
		   str_equal(channel1->key, channel2->key) &&
		   str_equal(channel1->topic, channel2->topic) &&
		   channel1->mode == channel2->mode &&
		   !memcmp(channel1->modes, channel2->modes, 255) &&
		   channel1->namreply_started == channel2->namreply_started &&
		   channel1->invitelist_started == channel2->invitelist_started &&
		   channel1->exceptlist_started == channel2->exceptlist_started &&
		   channel1->banlist_started == channel2->banlist_started &&
		   channel1->limit == channel2->limit &&
		   list_equal(channel1->nicks, channel2->nicks, (GEqualFunc)channel_nick_equal) &&
		   list_equal(channel1->banlist, channel2->banlist, (GEqualFunc)banlist_entry_equal) &&
		   list_equal(channel1->invitelist, channel2->invitelist, (GEqualFunc)str_equal) &&
		   list_equal(channel1->exceptlist, channel2->exceptlist, (GEqualFunc)str_equal);
}

static gboolean network_info_equal(const struct network_info *info1, const struct network_info *info2)
{
	null_equal(info1, info2);

	return str_equal(info1->name, info2->name) &&
		   str_equal(info1->server, info2->server) &&
		   hash_equal(info1->features, info2->features, (GEqualFunc)str_equal) &&
		   str_equal(info1->supported_user_modes, info2->supported_user_modes) &&
		   str_equal(info1->supported_channel_modes, info2->supported_channel_modes) &&
		   info1->casemapping == info2->casemapping &&
		   info1->channellen == info2->channellen &&
		   info1->topiclen == info2->topiclen;
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
		   list_equal(nick1->channel_nicks, nick2->channel_nicks, (GEqualFunc)channel_nick_equal);
}

static gboolean network_state_equal(const struct network_state *state1, const struct network_state *state2)
{
	null_equal(state1, state2);

	return network_nick_equal(&state1->me, &state2->me) &&
		   network_info_equal(state1->info, state2->info) &&
		   list_equal(state1->channels, state2->channels, (GEqualFunc)channel_state_equal) &&
		   list_equal(state1->nicks, state2->nicks, (GEqualFunc)network_nick_equal);
}

static struct ctrlproxy_config *my_config;

extern const struct linestack_ops linestack_file;

START_TEST(test_empty)
	struct network_state *ns1, *ns2;
	struct linestack_context *ctx;
	
	ns1 = network_state_init(NULL, "bla", "Gebruikersnaam", "Computernaam");
	ctx = create_linestack(&linestack_file, "test", my_config, ns1);

	ns2 = linestack_get_state(ctx, NULL);

	fail_unless (network_state_equal(ns1, ns2), "Network state returned not equal");
END_TEST

Suite *linestack_suite()
{
	Suite *s = suite_create("cmp");
	TCase *tc_core = tcase_create("core");
	my_config = g_new0(struct ctrlproxy_config, 1);
	my_config->config_dir = "/tmp";
	suite_add_tcase(s, tc_core);
	tcase_add_test(tc_core, test_empty);
	return s;
}
