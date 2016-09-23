/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2003 Jelmer Vernooĳ <jelmer@jelmer.uk>

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

#include "internals.h"
#include "irc.h"

enum mode_type { REMOVE = 0, ADD = 1 };

#define CHECK_ORIGIN(s, l, name) \
	if ((l)->origin == NULL) { \
		network_state_log(LOG_WARNING, (s), \
						  "Received "name" line without origin"); \
		return; \
	}

void network_nick_set_data(struct network_nick *n, const char *nick,
						   const char *username, const char *host)
{
	gboolean changed = FALSE;

	g_assert(n);
	g_assert(nick);

	if (!n->nick || strcmp(nick, n->nick) != 0) {
		g_free(n->nick); n->nick = g_strdup(nick);
		changed = TRUE;
	}

	g_assert(username);
	if (!n->username || strcmp(username, n->username) != 0) {
		g_free(n->username); n->username = g_strdup(username);
		changed = TRUE;
	}

	g_assert(host);
	if (!n->hostname || strcmp(host, n->hostname) != 0) {
		g_free(n->hostname); n->hostname = g_strdup(host);
		changed = TRUE;
	}

	if (changed) {
		g_free(n->hostmask);
		n->hostmask = g_strdup_printf("%s!%s@%s", nick, username, host);
	}
}

gboolean network_nick_set_nick(struct network_nick *n, const char *nick)
{
	g_assert(n != NULL);

	if (n->nick != NULL && !strcmp(nick, n->nick)) {
		return TRUE;
	}

	g_free(n->nick);
	n->nick = g_strdup(nick);

	g_free(n->hostmask);
	n->hostmask = g_strdup_printf("%s!%s@%s", nick, n->username, n->hostname);

	return TRUE;
}

gboolean network_nick_set_username(struct network_nick *n, const char *username)
{
	if (n == NULL)
		return FALSE;

	if (n->username != NULL && !strcmp(username, n->username))
		return TRUE;

	g_free(n->username);
	n->username = g_strdup(username);

	g_free(n->hostmask);
	n->hostmask = g_strdup_printf("%s!%s@%s", n->nick, n->username, n->hostname);

	return TRUE;
}

gboolean network_nick_set_hostname(struct network_nick *n, const char *hostname)
{
	if (n == NULL)
		return FALSE;

	if (n->hostname != NULL && !strcmp(hostname, n->hostname))
		return TRUE;

	g_free(n->hostname);
	n->hostname = g_strdup(hostname);

	g_free(n->hostmask);
	n->hostmask = g_strdup_printf("%s!%s@%s", n->nick, n->username, n->hostname);

	return TRUE;
}

gboolean network_nick_set_hostmask(struct network_nick *n, const char *hm)
{
	char *t, *u;

	if (n == NULL)
		return FALSE;

	if (hm == NULL)
		return FALSE;

	if (n->hostmask && !strcmp(n->hostmask, hm))
		return TRUE;

	g_free(n->hostmask);
	g_free(n->nick); n->nick = NULL;
	g_free(n->username); n->username = NULL;
	g_free(n->hostname); n->hostname = NULL;
	n->hostmask = g_strdup(hm);

	t = strchr(hm, '!');
	if (!t)
		return FALSE;
	n->nick = g_strndup(hm, t-hm);

	u = strchr(t, '@');
	if (!u)
		return FALSE;
	n->username = g_strndup(t+1, u-t-1);

	n->hostname = g_strdup(u+1);

	return TRUE;
}

static void free_channel_nick(struct channel_nick *n)
{
	g_assert(n);

	g_assert(n->channel);
	g_assert(n->global_nick);

	n->channel->nicks = g_list_remove(n->channel->nicks, n);
	n->global_nick->channel_nicks = g_list_remove(n->global_nick->channel_nicks, n);

	if (g_list_length(n->global_nick->channel_nicks) == 0 && n->global_nick->query == 0)
		free_network_nick(n->channel->network, n->global_nick);

	g_free(n->last_flags);
	g_free(n);
}

gboolean nicklist_add_entry(GList **nicklist, const char *opt_arg,
								   const char *by_nick, time_t at)
{
	struct nicklist_entry *be;
	be = g_new0(struct nicklist_entry, 1);
	be->time_set = at;
	be->hostmask = g_strdup(opt_arg);
	be->by = (by_nick?g_strdup(by_nick):NULL);
	g_assert(be != NULL);
	*nicklist = g_list_append(*nicklist, be);

	return TRUE;
}

gboolean nicklist_remove_entry(GList **nicklist, const char *hostmask)
{
	struct nicklist_entry *be;

	be = find_nicklist_entry(*nicklist, hostmask);
	if (be == NULL)
		return FALSE;
	*nicklist = g_list_remove(*nicklist, be);
	free_nicklist_entry(be);
	return TRUE;
}

void free_nicklist_entry(struct nicklist_entry *be)
{
	g_free(be->hostmask);
	g_free(be->by);
	g_free(be);
}

struct nicklist_entry *find_nicklist_entry(GList *entries, const char *hostmask)
{
	GList *gl;
	for (gl = entries; gl; gl = g_list_next(gl)) {
		struct nicklist_entry *be = gl->data;
		if (!strcmp(be->hostmask, hostmask))
			return be;
	}
	return NULL;
}

void free_nicklist(GList **nicklist)
{
	GList *g;

	g = *nicklist;
	while(g) {
		struct nicklist_entry *be = g->data;
		g = g_list_remove(g, be);
		free_nicklist_entry(be);
	}

	*nicklist = NULL;
}

static void free_names(struct irc_channel_state *c)
{
	while(c->nicks) {
		free_channel_nick((struct channel_nick *)c->nicks->data);
	}
	c->nicks = NULL;
}

void free_channel_state(struct irc_channel_state *c)
{
	int i;
	if (c == NULL)
		return;
	free_names(c);
	g_free(c->name);
	g_free(c->topic);
	g_free(c->topic_set_by);
	if (c->network != NULL) {
		c->network->channels = g_list_remove(c->network->channels, c);
		c->network = NULL;
	}
	for (i = 0; i < MAXMODES; i++)
		g_free(c->chanmode_option[i]);
	for (i = 0; i < MAXMODES; i++)
		free_nicklist(&c->chanmode_nicklist[i]);
	g_free(c);
}

struct irc_channel_state *find_channel(struct irc_network_state *st, const char *name)
{
	GList *cl;
	g_assert(st);
	g_assert(name);
	for (cl = st->channels; cl; cl = g_list_next(cl)) {
		struct irc_channel_state *c = (struct irc_channel_state *)cl->data;

		if (!irccmp(st->info, c->name, name))
			return c;
	}
	return NULL;
}

struct irc_channel_state *irc_channel_state_new(const char *name)
{
	struct irc_channel_state *c;
	g_assert(name != NULL);

	c = g_new0(struct irc_channel_state ,1);
	c->name = g_strdup(name);

	return c;
}

struct irc_channel_state *find_add_channel(struct irc_network_state *st, char *name)
{
	struct irc_channel_state *c;
	g_assert(st);
	g_assert(name);

	c = find_channel(st, name);
	if (c != NULL)
		return c;
	c = irc_channel_state_new(name);
	c->network = st;
	g_assert(c != NULL);
	st->channels = g_list_append(st->channels, c);

	return c;
}

/**
 * Find channel nick by name
 *
 * @param c Channel state to search
 * @param name Name of the nick to search for
 * @return NULL if not found, channel_nick if found
 */
struct channel_nick *find_channel_nick(struct irc_channel_state *c,
									   const char *name)
{
	GList *l;
	const char *realname = name;
	const struct irc_network_info *info;

	g_assert(name);
	g_assert(c != NULL);
	if (c->network == NULL)
		info = NULL;
	else
		info = c->network->info;

	if (is_prefix(realname[0], info))
		realname++;

	for (l = c->nicks; l; l = g_list_next(l)) {
		struct channel_nick *n = (struct channel_nick *)l->data;
		if (!irccmp(info, n->global_nick->nick, realname))
			return n;
	}

	return NULL;
}

/**
 * Find channel nick by hostmask
 *
 * @param c Channel state to search
 * @param hm Hostmask of the nick to search for
 * @return NULL if not found, channel_nick if found
 */
struct channel_nick *find_channel_nick_hostmask(struct irc_channel_state *c,
									   		    const char *hm)
{
	GList *l;
	g_assert(hm);
	g_assert(c != NULL);

	g_assert(c->network);
	for (l = c->nicks; l; l = g_list_next(l)) {
		struct channel_nick *n = (struct channel_nick *)l->data;
		if (!irccmp(c->network->info, n->global_nick->hostmask, hm))
			return n;
	}

	return NULL;
}

/**
 * Find network nick by name
 *
 * @param n Network state to search
 * @param name Name of the nick to search for
 * @return NULL if not found, network_nick otherwise
 */
struct network_nick *find_network_nick(struct irc_network_state *n,
									   const char *name)
{
	GList *gl;

	g_assert(name);
	g_assert(n);

	if (!irccmp(n->info, n->me.nick, name))
		return &n->me;

	for (gl = n->nicks; gl; gl = g_list_next(gl)) {
		struct network_nick *ndd = (struct network_nick*)gl->data;
		if (!irccmp(n->info, ndd->nick, name)) {
			return ndd;
		}
	}

	return NULL;
}

/**
 * Search for a network nick, or add it if not found.
 *
 * @param n Network state to search
 * @param name Name of the nick to search for
 * @return network_nick structure, or NULL if out of memory.
 */
struct network_nick *find_add_network_nick(struct irc_network_state *n,
										   const char *name)
{
	struct network_nick *nd;

	g_assert(name != NULL);
	g_assert(n != NULL);

	nd = find_network_nick(n, name);
	if (nd != NULL)
		return nd;

	/* create one, if it doesn't exist */
	nd = g_new0(struct network_nick,1);
	g_assert(nd != NULL);
	g_assert(!is_prefix(name[0], n->info));
	nd->nick = g_strdup(name);
	nd->hops = -1;

	n->nicks = g_list_append(n->nicks, nd);
	return nd;
}

/**
 * Search for a channel nick, or add it if not found.
 *
 * @param n Channel state to search
 * @param name Name of the nick to search for
 * @return channel_nick structure, or NULL if out of memory.
 */
struct channel_nick *find_add_channel_nick(struct irc_channel_state *c,
										   const char *name)
{
	struct channel_nick *n;
	const char *realname = name;
	char prefix = 0;

	g_assert(c);
	g_assert(name);
	g_assert(strlen(name) > 0);
	g_assert(c->network);

	if (is_prefix(realname[0], c->network->info)) {
		prefix = realname[0];
		realname++;
	}

	n = find_channel_nick(c, realname);
	if (n != NULL)
		return n;

	n = g_new0(struct channel_nick,1);
	g_assert(n != NULL);

	n->channel = c;
	n->global_nick = find_add_network_nick(c->network, realname);
	if (prefix != 0) {
		char mode = get_mode_by_prefix(prefix, c->network->info);
		if (mode)
			modes_set_mode(n->modes, mode);
    }
	c->nicks = g_list_append(c->nicks, n);
	n->global_nick->channel_nicks = g_list_append(n->global_nick->channel_nicks, n);
	return n;
}

static void handle_join(struct irc_network_state *s, const struct irc_line *l)
{
	struct irc_channel_state *c;
	struct channel_nick *ni;
	int i;
	char **channels;
	char *nick;

	CHECK_ORIGIN(s, l, "NICK");

	nick = line_get_nick(l);

	g_assert(nick != NULL);

	channels = g_strsplit(l->args[1], ",", 0);
	if (channels == NULL) {
		network_state_log(LOG_ERROR, s, "Unable to split channel list (%s)",
						  l->args[1]);
		return;
	}

	for (i = 0; channels[i]; i++) {
		/* Someone is joining a channel the user is on */
		c = find_add_channel(s, channels[i]);
		g_assert(s->channels != NULL);
		ni = find_add_channel_nick(c, nick);
		network_nick_set_hostmask(ni->global_nick, l->origin);

		/* The user is joining a channel */
		if (!irccmp(s->info, nick, s->me.nick)) {
			network_state_log(LOG_TRACE, s, "Joining channel %s", c->name);
		} else {
			network_state_log(LOG_TRACE, s, "%s joins channel %s", nick,
							  c->name);
		}
	}
	g_free(nick);
	g_strfreev(channels);
}


static void handle_part(struct irc_network_state *s, const struct irc_line *l)
{
	struct irc_channel_state *c;
	struct channel_nick *n;
	char **channels;
	int i;
	char *nick;

	CHECK_ORIGIN(s, l, "PART");

	nick = line_get_nick(l);

	if (nick == NULL) {
		return;
	}

	channels = g_strsplit(l->args[1], ",", 0);
	if (channels == NULL) {
		network_state_log(LOG_ERROR, s, "Unable to split channel list (%s)",
						  l->args[1]);
		return;
	}

	for (i = 0; channels[i]; i++) {
		c = find_channel(s, channels[i]);

		if (c == NULL) {
			network_state_log(LOG_WARNING, s,
					"Can't part or let other nick part %s(unknown channel)",
					channels[i]);
			continue;
		}

		n = find_channel_nick(c, nick);
		if (n != NULL) {
			free_channel_nick(n);
		} else {
			network_state_log(LOG_WARNING, s,
				"Can't remove nick %s from channel %s: nick not on channel",
				nick, channels[i]);
		}

		if (!irccmp(s->info, nick, s->me.nick) && c) {
			network_state_log(LOG_TRACE, s, "Leaving %s", channels[i]);
			free_channel_state(c);
		} else {
			network_state_log(LOG_TRACE, s, "%s leaves %s", nick, channels[i]);
		}
	}
	g_free(nick);
	g_strfreev(channels);
}

static void handle_kick(struct irc_network_state *s, const struct irc_line *l)
{
	struct irc_channel_state *c;
	struct channel_nick *n;
	char **channels, **nicks;
	int i;
	char *nick;

	CHECK_ORIGIN(s, l, "KICK");

	nick = line_get_nick(l);

	channels = g_strsplit(l->args[1], ",", 0);
	if (channels == NULL) {
		g_free(nick);
		network_state_log(LOG_ERROR, s, "Unable to split channel list (%s)",
						  l->args[1]);
		return;
	}

	nicks = g_strsplit(l->args[2], ",", 0);
	if (nicks == NULL) {
		g_free(nick);
		g_strfreev(channels);
		network_state_log(LOG_ERROR, s, "Unable to split nick list (%s)",
						  l->args[2]);
		return;

	}
	for (i = 0; channels[i] && nicks[i]; i++) {
		c = find_channel(s, channels[i]);

		if (c == NULL){
			network_state_log(LOG_WARNING, s, "Can't kick nick %s from unknown channel %s", nicks[i], channels[i]);
			continue;
		}

		n = find_channel_nick(c, nicks[i]);
		if (n == NULL) {
			network_state_log(LOG_WARNING, s, "Can't kick nick %s from channel %s: nick not on channel", nicks[i], channels[i]);
			continue;
		}

		free_channel_nick(n);

		if (!irccmp(s->info, nicks[i], s->me.nick)) {
			network_state_log(LOG_INFO, s, "Kicked off %s by %s", c->name, nick);
			free_channel_state(c);
		} else {
			network_state_log(LOG_TRACE, s, "%s kicked off %s by %s", nicks[i], channels[i], nick);
		}
	}

	if (channels[i] != NULL || nicks[i] != NULL) {
		network_state_log(LOG_WARNING, s,
					  "KICK command has unequal number of channels and nicks");
	}

	g_free(nick);
	g_strfreev(nicks);
	g_strfreev(channels);
}

static void handle_topic(struct irc_network_state *s, const struct irc_line *l)
{
	struct irc_channel_state *c = find_channel(s, l->args[1]);

	CHECK_ORIGIN(s, l, "TOPIC");

	if (c->topic != NULL) {
		g_free(c->topic);
	}
	if (c->topic_set_by != NULL) {
		g_free(c->topic_set_by);
	}
	c->topic = g_strdup(l->args[2]);
	c->topic_set_time = time(NULL);
	c->topic_set_by = line_get_nick(l);
}

static void handle_332(struct irc_network_state *s, const struct irc_line *l)
{
	struct irc_channel_state *c = find_channel(s, l->args[2]);

	if (c == NULL) {
		network_state_log(LOG_WARNING, s,
					"Can't set topic for unknown channel '%s'!", l->args[2]);
		return;
	}

	g_free(c->topic);
	c->topic = g_strdup(l->args[3]);
}

static void handle_333(struct irc_network_state *s, const struct irc_line *l)
{
	struct irc_channel_state *c = find_channel(s, l->args[2]);

	if (!c) {
		network_state_log(LOG_WARNING, s,
				"Can't set topic last set time for unknown channel '%s'!",
				l->args[2]);
		return;
	}

	c->topic_set_time = strtol(l->args[4], NULL, 0);
	c->topic_set_by = g_strdup(l->args[3]);
}

static void handle_no_topic(struct irc_network_state *s, const struct irc_line *l)
{
	struct irc_channel_state *c = find_channel(s, l->args[1]);

	if (c == NULL) {
		network_state_log(LOG_WARNING, s,
					"Can't unset topic for unknown channel '%s'!", l->args[2]);
		return;
	}

	g_free(c->topic);
	c->topic = NULL;
}

static void handle_namreply(struct irc_network_state *s, const struct irc_line *l)
{
	gchar **names;
	int i;
	struct irc_channel_state *c = find_channel(s, l->args[3]);

	if (c == NULL) {
		network_state_log(LOG_WARNING, s,
					"Can't add names to %s: channel not found", l->args[3]);
		return;
	}

	names = g_strsplit(l->args[4], " ", -1);
	if (names == NULL) {
		network_state_log(LOG_ERROR, s, "Unable to split names list (%s)",
						  l->args[4]);
		return;
	}

	c->mode = l->args[2][0];
	if (!c->namreply_started) {
		free_names(c);
		c->namreply_started = TRUE;
	}

	for (i = 0; names[i]; i++) {
		if (strlen(names[i]) == 0) {
			continue;
		}
		find_add_channel_nick(c, names[i]);
	}
	g_strfreev(names);
}

static void handle_end_names(struct irc_network_state *s, const struct irc_line *l)
{
	struct irc_channel_state *c = find_channel(s, l->args[2]);
	if (c != NULL)
		c->namreply_started = FALSE;
	else
		network_state_log(LOG_WARNING, s,
				  "Can't end /NAMES command for %s: channel not found",
				  l->args[2]);
}

static void handle_invitelist_entry(struct irc_network_state *s, const struct irc_line *l)
{
	struct irc_channel_state *c = find_channel(s, l->args[2]);
	GList **list;

	if (c == NULL) {
		network_state_log(LOG_WARNING, s,
				"Can't add invitelist entries to %s: channel not found",
				l->args[2]);
		return;
	}

	list = &channel_mode_nicklist(c, 'I');

	if (!c->invitelist_started) {
		free_nicklist(list);
		c->invitelist_started = TRUE;
	}

	nicklist_add_entry(list, l->args[3], NULL, 0);
}

static void handle_end_invitelist(struct irc_network_state *s, const struct irc_line *l)
{
	struct irc_channel_state *c = find_channel(s, l->args[2]);
	if (c == NULL) {
		network_state_log(LOG_WARNING, s,
			  "Can't end invitelist for %s: channel not found", l->args[2]);
		return;
	}

	c->invitelist_started = FALSE;
	channel_mode_nicklist_present(c,'I') = TRUE;
}

static void handle_exceptlist_entry(struct irc_network_state *s, const struct irc_line *l)
{
	struct irc_channel_state *c = find_channel(s, l->args[2]);
	GList **list;

	if (c == NULL) {
		network_state_log(LOG_WARNING, s,
				"Can't add exceptlist entries to %s: channel not found",
				l->args[2]);
		return;
	}

	list = &channel_mode_nicklist(c, 'e');

	if (!c->exceptlist_started) {
		free_nicklist(list);
		c->exceptlist_started = TRUE;
	}

	nicklist_add_entry(list, l->args[3], NULL, 0);
}

static void handle_end_exceptlist(struct irc_network_state *s, const struct irc_line *l)
{
	struct irc_channel_state *c = find_channel(s, l->args[2]);
	if (c == NULL) {
		network_state_log(LOG_WARNING, s,
			"Can't end exceptlist for %s: channel not found", l->args[2]);
		return;
	}
	c->exceptlist_started = FALSE;
	channel_mode_nicklist_present(c, 'e') = TRUE;
}



static void handle_banlist_entry(struct irc_network_state *s, const struct irc_line *l)
{
	struct irc_channel_state *c = find_channel(s, l->args[2]);
	GList **list;

	if (c == NULL) {
		network_state_log(LOG_WARNING, s,
					"Can't add banlist entries to %s: channel not found",
					l->args[2]);
		return;
	}

	list = &channel_mode_nicklist(c, 'b');

	if (!c->banlist_started) {
		free_nicklist(list);
		c->banlist_started = TRUE;
	}

	nicklist_add_entry(list, l->args[3], l->args[4],
					   (l->args[4] && l->args[5])?atol(l->args[5]):0);
}

static void handle_end_banlist(struct irc_network_state *s, const struct irc_line *l)
{
	struct irc_channel_state *c = find_channel(s, l->args[2]);

	if (c == NULL) {
		network_state_log(LOG_WARNING, s,
				"Can't end banlist for %s: channel not found", l->args[2]);
		return;
	}

	c->banlist_started = FALSE;
	channel_mode_nicklist_present(c,'b') = TRUE;
}

static void handle_whoreply(struct irc_network_state *s, const struct irc_line *l)
{
	struct irc_channel_state *cs;
	struct network_nick *nn;
	struct channel_nick *cn;
	char *fullname;

	nn = find_add_network_nick(s, l->args[6]);
	g_assert(nn != NULL);
	network_nick_set_data(nn, l->args[6], l->args[3], l->args[4]);

	fullname = NULL;
	nn->hops = strtol(l->args[8], &fullname, 10);
	g_assert(fullname);

	if (nn->fullname == NULL) {
		if (fullname[0] == ' ')
			fullname++;

		g_free(nn->fullname);
		nn->fullname = g_strdup(fullname);
	}

	g_free(nn->server);
	nn->server = g_strdup(l->args[5]);

	cs = find_channel(s, l->args[2]);
	if (cs == NULL)
		return;

	cn = find_channel_nick(cs, nn->nick);

	if (cn == NULL) {
		network_state_log(LOG_WARNING,
						  s,
						  "User %s in WHO reply not in expected channel %s!",
						  nn->nick, l->args[2]);
		return;
	}

	g_free(cn->last_flags);
	cn->last_flags = g_strdup(l->args[7]);

	cn->last_update = time(NULL);
}

static void handle_end_who(struct irc_network_state *s, const struct irc_line *l)
{
}

static void handle_nowaway(struct irc_network_state *s, const struct irc_line *l)
{
	s->is_away = TRUE;
}

static void handle_unaway(struct irc_network_state *s, const struct irc_line *l)
{
	s->is_away = FALSE;
}

static void handle_quit(struct irc_network_state *s, const struct irc_line *l)
{
	char *nick;
	struct network_nick *nn;

	CHECK_ORIGIN(s, l, "QUIT");

	nick = line_get_nick(l);
	nn = find_network_nick(s, nick);
	g_free(nick);

	if (nn == &s->me) {
		while (nn->channel_nicks) {
			struct channel_nick *n = nn->channel_nicks->data;
			free_channel_nick(n);
		}
	} else if (nn != NULL)
		free_network_nick(s, nn);
}

gboolean modes_change_mode(irc_modes_t modes, gboolean set, char newmode)
{
	if (modes[(unsigned char)newmode] == set)
		return FALSE;

	modes[(unsigned char)newmode] = set;

	return TRUE;
}

static int channel_state_change_mode(struct irc_network_state *s, struct network_nick *by, struct irc_channel_state *c, gboolean set, char mode, const char *opt_arg)
{
	struct irc_network_info *info = s->info;

	enum chanmode_type cmt = network_chanmode_type(mode, s->info);

	if (cmt == CHANMODE_NICKLIST) {
		if (opt_arg == NULL) {
			network_state_log(LOG_WARNING, s, "Missing argument for %c MODE set/unset", mode);
			return -1;
		}

		if (channel_mode_nicklist_present(c, mode)) {
			if (set) {
				nicklist_add_entry(&channel_mode_nicklist(c, mode), opt_arg,
								   by?by->nick:NULL, time(NULL));
			} else {
				if (!nicklist_remove_entry(&channel_mode_nicklist(c, mode), opt_arg))  {
					network_state_log(LOG_WARNING, s, "Unable to remove nonpresent %c MODE entry '%s' on %s", mode, opt_arg, c->name);
					return 1;
				}
			}
		}
		return 1;
	} else if (cmt == CHANMODE_OPT_SETTING || cmt == CHANMODE_SETTING) {
		if (set) {
			if (opt_arg == NULL) {
				network_state_log(LOG_WARNING, s, "Missing argument for MODE %c set", mode);
				return -1;
			}

			g_free(channel_mode_option(c, mode));
			channel_mode_option(c, mode) = g_strdup(opt_arg);

			return 1;
		} else {
			g_free(channel_mode_option(c, mode));
			channel_mode_option(c, mode) = NULL;

			return 0;
		}
	} else if (is_prefix_mode(info, mode)) {
		struct channel_nick *n;

		if (opt_arg == NULL) {
			network_state_log(LOG_WARNING, s, "Mode %c requires nick argument, but no argument found", mode);
			return -1;
		}

		n = find_channel_nick(c, opt_arg);
		if (!n) {
			network_state_log(LOG_WARNING, s, "Can't set mode %c%c on nick %s on channel %s, because nick does not exist!", set?'+':'-', mode, opt_arg, c->name);
			return -1;
		}
		if (set) {
			modes_set_mode(n->modes, mode);
		} else {
			modes_unset_mode(n->modes, mode);
		}
		return 1;
	} else if (cmt == CHANMODE_BOOL) {
		modes_change_mode(c->modes, set, mode);
		return 0;
	} else {
		network_state_log(LOG_WARNING, s, "Mode '%c' set on channel %s is not in the supported list of channel modes from the server", mode, c->name);
		return 1;
	}
}

static void handle_mode(struct irc_network_state *s, const struct irc_line *l)
{
	/* Format:
	 * MODE %|<nick>|<channel> [<mode> [<mode parameters>]] */
	gboolean t = TRUE;
	int i;
	int arg = 3;

	g_assert(s != NULL);

	/* Channel modes */
	if (is_channelname(l->args[1], s->info)) {
		struct irc_channel_state *c = find_channel(s, l->args[1]);
		struct network_nick *by;
		int ret;
		char *by_name;

		if (c == NULL) {
			network_state_log(LOG_WARNING, s,
				"Unable to change mode for unknown channel '%s'", l->args[1]);
			return;
		}

		by_name = line_get_nick(l);
		by = find_network_nick(s, by_name);
		g_free(by_name);

		for(i = 0; l->args[2][i]; i++) {
			switch(l->args[2][i]) {
				case '+': t = TRUE; break;
				case '-': t = FALSE; break;
				default:
					  ret = channel_state_change_mode(s, by, c, t,
													  l->args[2][i],
													  l->args[arg]);
					  if (ret == -1)
						  return;
					  g_assert(!(ret > 1));
					  if (l->args[arg] != NULL)
						  arg += ret;
					  break;
			}
		}

		/* User modes */
	} else {
		struct network_nick *nn = find_add_network_nick(s, l->args[1]);

		for(i = 0; l->args[2][i]; i++) {
			switch(l->args[2][i]) {
				case '+': t = TRUE;break;
				case '-': t = FALSE; break;
				default:
					  modes_change_mode(nn->modes, t, l->args[2][i]);
					  break;
			}
		}
	}

	if (arg > l->argc) {
		network_state_log(LOG_WARNING, s,
						  "not enough arguments for mode change %s on %s",
						  l->args[2], l->args[1]);
	} else if (l->args[arg] != NULL && strcmp(l->args[arg], "") != 0) {
		network_state_log(LOG_WARNING, s,
						  "mode %s %s argument not consumed: %s", l->args[2],
						  l->args[1],
						  l->args[arg]);
	}
}

static void handle_001(struct irc_network_state *s, const struct irc_line *l)
{
	g_free(s->me.nick);
	s->me.nick = g_strdup(l->args[1]);
}

static void handle_004(struct irc_network_state *s, const struct irc_line *l)
{
	s->info->supported_user_modes = g_strdup(l->args[4]);
	s->info->supported_channel_modes = g_strdup(l->args[5]);
	s->info->server = g_strdup(l->args[2]);
}

static void handle_privmsg(struct irc_network_state *s, const struct irc_line *l)
{
	struct network_nick *nn;
	char *nick;

	CHECK_ORIGIN(s, l, "PRIVMSG");

	if (irccmp(s->info, l->args[1], s->me.nick) != 0) {
		return;
	}

	nick = line_get_nick(l);
	nn = find_add_network_nick(s, nick);
	g_free(nick);

	if (nn != NULL) {
		nn->query = 1;
	}
}

static void handle_nick(struct irc_network_state *s, const struct irc_line *l)
{
	struct network_nick *nn;
	char *nick;

	CHECK_ORIGIN(s, l, "NICK");

	nick = line_get_nick(l);
	nn = find_add_network_nick(s, nick);
	g_free(nick);

	if (nn != NULL) {
		network_nick_set_nick(nn, l->args[1]);
	}
}

static void handle_umodeis(struct irc_network_state *s, const struct irc_line *l)
{
	if (!string2mode(l->args[1], s->me.modes)) {
		network_state_log(LOG_WARNING, s,
						  "Invalid MODE change %s for user", l->args[1]);
	}
}

static void handle_324(struct irc_network_state *s, const struct irc_line *l)
{
	struct irc_channel_state *ch = find_channel(s, l->args[2]);

	if (ch == NULL) {
		network_state_log(LOG_WARNING, s,
			"Can't store modes for %s: channel not found", l->args[2]);
		return;
	}

	if (!string2mode(l->args[3], ch->modes)) {
		network_state_log(LOG_WARNING, s,
						  "Invalid MODE change %s for channel %s", l->args[3], l->args[2]);
	}

	ch->mode_received = TRUE;
}

static void handle_329(struct irc_network_state *s, const struct irc_line *l)
{
	struct irc_channel_state *ch = find_channel(s, l->args[2]);

	if (ch == NULL) {
		network_state_log(LOG_WARNING, s,
			"Can't store creationtime for %s: channel not found", l->args[2]);
		return;
	}

	ch->creation_time = atol(l->args[3]);
}

static void handle_302(struct irc_network_state *s, const struct irc_line *l)
{
	int i;
	gchar **users = g_strsplit(g_strstrip(l->args[2]), " ", 0);
	if (users == NULL) {
		network_state_log(LOG_ERROR, s, "Unable to split user list (%s)",
						  l->args[2]);
		return;
	}

	for (i = 0; users[i]; i++) {
		/* We got a USERHOST response, split it into nick and user@host, and check the nick */
		gchar** tmp302 = g_strsplit(users[i], "=", 2);
		if (g_strv_length(tmp302) > 1) {
			char *hm;
			struct network_nick *nn;
			/* FIXME: Strip *'s from the end of tmp302[0]*/
			nn = find_add_network_nick(s, tmp302[0]);
			hm = g_strdup_printf("%s!%s", tmp302[0], tmp302[1]+1);
			network_nick_set_hostmask(nn, hm);
			g_free(hm);
		}
		g_strfreev(tmp302);
	}

	g_strfreev(users);
}

extern void handle_005(struct irc_network_state *s, const struct irc_line *l);

static struct irc_command {
	char *command;
	int min_args;
	void (*handler) (struct irc_network_state *s, const struct irc_line *l);
} irc_commands[] = {
	{ "JOIN", 1, handle_join },
	{ "PART", 1, handle_part },
	{ "KICK", 2, handle_kick },
	{ "QUIT", 0, handle_quit },
	{ "TOPIC", 2, handle_topic },
	{ "NICK", 1, handle_nick },
	{ "PRIVMSG", 2, handle_privmsg },
	{ "MODE", 2, handle_mode },
	{ "001", 1, handle_001 },
	{ "004", 5, handle_004 },
	{ "005", 3, handle_005 },
	{ "221", 1, handle_umodeis },
	{ "302", 2, handle_302 },
	{ "324", 3, handle_324 },
	{ "329", 3, handle_329 },
	{ "332", 3, handle_332 },
	{ "333", 3, handle_333 },
	{ "331", 1, handle_no_topic },
	{ "353", 4, handle_namreply },
	{ "366", 2, handle_end_names },
	{ "367", 2, handle_banlist_entry },
	{ "368", 2, handle_end_banlist },
	{ "346", 2, handle_invitelist_entry },
	{ "347", 2, handle_end_invitelist },
	{ "348", 2, handle_exceptlist_entry },
	{ "349", 2, handle_end_exceptlist },
	{ "352", 8, handle_whoreply },
	{ "315", 1, handle_end_who },
	{ "306", 1, handle_nowaway },
	{ "305", 1, handle_unaway },
	{ NULL }
};

gboolean state_handle_data(struct irc_network_state *s, const struct irc_line *l)
{
	int i,j;

	if (s == NULL || l == NULL || l->args == NULL || l->args[0] == NULL)
		return FALSE;

	for (i = 0; irc_commands[i].command; i++) {
		if (!base_strcmp(irc_commands[i].command, l->args[0])) {
			for (j = 0; j <= irc_commands[i].min_args; j++) {
				if (l->args[j] == NULL)
					return FALSE;
			}
			irc_commands[i].handler(s,l);
			return TRUE;
		}
	}

	return FALSE;
}

struct irc_network_state *network_state_init(const char *nick,
										 const char *username,
										 const char *hostname)
{
	struct irc_network_state *state = g_new0(struct irc_network_state, 1);
	state->me.query = 1;
	network_nick_set_data(&state->me, nick, username, hostname);
	state->info = network_info_init();

	return state;
}

void free_network_nick(struct irc_network_state *st, struct network_nick *nn)
{
	g_assert(st == NULL || nn != &st->me);
	g_assert(nn);

	/* No recursion please... */
	nn->query = 1;

	while (nn->channel_nicks) {
		struct channel_nick *n = nn->channel_nicks->data;
		free_channel_nick(n);
	}

	g_free(nn->hostmask);
	g_free(nn->username);
	g_free(nn->hostname);
	g_free(nn->fullname);
	g_free(nn->server);
	g_free(nn->nick);
	if (st != NULL)
		st->nicks = g_list_remove(st->nicks, nn);
	g_free(nn);
}

void free_network_state(struct irc_network_state *state)
{
	if (state == NULL)
		return;

	while (state->channels != NULL)
		free_channel_state((struct irc_channel_state *)state->channels->data);

	g_free(state->me.nick);
	g_free(state->me.username);
	g_free(state->me.hostname);
	g_free(state->me.hostmask);

	while (state->nicks != NULL)
	{
		struct network_nick *nn = state->nicks->data;
		free_network_nick(state, nn);
	}

	free_network_info(state->info);
	g_free(state);
}

void network_state_log(enum log_level l,
					   const struct irc_network_state *st, const char *fmt, ...)
{
	char *ret;
	va_list ap;

	if (st->log == NULL)
		return;

	g_assert(st);
	g_assert(fmt);

	va_start(ap, fmt);
	ret = g_strdup_vprintf(fmt, ap);
	va_end(ap);

	st->log(l, st->userdata, ret);

	g_free(ret);
}

void network_state_set_log_fn(struct irc_network_state *st,
							  void (*fn) (enum log_level, void *, const char *),
							  void *userdata)
{
	st->log = fn;
	st->userdata = userdata;
}

gboolean string2mode(const char *modes, irc_modes_t ar)
{
	gboolean action = TRUE;
	modes_clear(ar);

	if (modes == NULL)
		return TRUE;

	if (strlen(modes) == 0)
		return TRUE;

	if (modes[0] != '+' && modes[0] != '-')
		return FALSE;

	for (; *modes; modes++) {
		switch (*modes) {
			case '+': action = TRUE; break;
			case '-': action = FALSE; break;
			default: modes_change_mode(ar, action, *modes); break;
		}
	}

	return TRUE;
}

char *mode2string(irc_modes_t modes)
{
	char ret[256];
	unsigned char i;
	int pos = 0;
	ret[0] = '\0';
	for(i = 0; i < MAXMODES; i++) {
		if (modes[i]) { ret[pos] = (char)i; pos++; }
	}
	ret[pos] = '\0';

	if (strlen(ret) == 0) {
		return NULL;
	} else {
		return g_strdup_printf("+%s", ret);
	}
}

gboolean is_prefix_mode(const struct irc_network_info *info, char mode)
{
	return get_prefix_by_mode(mode, info) != ' ';
}

gboolean line_from_nick(const struct irc_network_info *info, const struct irc_line *l, const char *nick)
{
	char *line_nick = line_get_nick(l);
	gboolean ret;

	ret = (irccmp(info, nick, line_nick) == 0);

	g_free(line_nick);

	return ret;
}
