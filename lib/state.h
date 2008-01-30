/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2005 Jelmer Vernooij <jelmer@nl.linux.org>

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

#ifndef __CTRLPROXY_STATE_H__
#define __CTRLPROXY_STATE_H__

#include "isupport.h"
#include "log.h"

typedef gboolean irc_modes_t[255];

/**
 * @file
 * @brief State information
 */

struct network;
struct client;
struct line;

/* When changing one of these structs, also change the marshalling
 * function for that struct in state.c */

/**
 * Record of a nick on a channel.
 */
struct channel_nick {
	irc_modes_t modes;
	struct network_nick *global_nick;
	struct channel_state *channel;

	/* This information is not always set and may change */
	time_t last_update; /* last time this section was updated */
	char *last_flags; /* whether the user is an oper, away, etc */
};

struct network_nick {
	/* Whether notifications are received for this nick */
	gboolean query;
	char *nick;
	char *fullname;
	char *username;
	char *hostname;
	char *hostmask;
	irc_modes_t modes;
	char *server;
	GList *channel_nicks;

	int hops; /* IRC hops from user to this user */
};

/**
 * An entry in the banlist of a channel.
 */
struct banlist_entry {
	char *hostmask;
	char *by;
	time_t time_set;
};

/**
 * The state of a particular channel.
 */
struct channel_state {
	char *name;
	char *key;
	char *topic;
	time_t topic_set_time;
	char *topic_set_by; /* nickname */
	char mode; /* Private, secret, etc */
	irc_modes_t modes;
	time_t creation_time;
	gboolean namreply_started;
	gboolean banlist_started;
	gboolean invitelist_started;
	gboolean exceptlist_started;
	gboolean mode_received;
	int limit;
	GList *nicks;
	GList *banlist;
	GList *invitelist;
	GList *exceptlist;
	struct network_state *network;
};

/**
 * Describes the (partial) state of a network at a specific time
 */
struct network_state 
{
	void *userdata;
	void (*log) (enum log_level l,
				 void *userdata,
				 const char *msg);
	GList *channels;
	GList *nicks;
	struct network_nick me;
	struct network_info info;
	gboolean is_away;
};

/* state.c */
G_GNUC_MALLOC G_MODULE_EXPORT struct network_state *network_state_init(const char *nick, const char *username, const char *hostname);
G_MODULE_EXPORT void free_network_state(struct network_state *);
G_MODULE_EXPORT gboolean state_handle_data(struct network_state *s, const struct line *l);

G_MODULE_EXPORT struct channel_state *find_channel(struct network_state *st, const char *name);
G_MODULE_EXPORT struct channel_nick *find_channel_nick(struct channel_state *c, const char *name);
G_MODULE_EXPORT struct channel_nick *find_channel_nick_hostmask(struct channel_state *c, const char *hostmask);
G_MODULE_EXPORT struct channel_nick *find_add_channel_nick(struct channel_state *c, const char *name);
G_MODULE_EXPORT struct network_nick *find_network_nick(struct network_state *c, const char *name);
G_MODULE_EXPORT gboolean network_nick_set_hostmask(struct network_nick *n, const char *hm);
G_MODULE_EXPORT gboolean client_send_state(struct client *, struct network_state *);
G_MODULE_EXPORT void network_state_log(enum log_level l, const struct network_state *st, const char *fmt, ...);
G_MODULE_EXPORT void network_state_set_log_fn(struct network_state *st, void (*fn) (enum log_level, void *, const char *), void *userdata);

G_MODULE_EXPORT G_GNUC_MALLOC char *mode2string(irc_modes_t modes);
G_MODULE_EXPORT void string2mode(char *modestring, irc_modes_t modes);
G_MODULE_EXPORT gboolean modes_change_mode(irc_modes_t modes, gboolean set, char newmode);
#define modes_set_mode(modes, newmode) modes_change_mode(modes, TRUE, newmode)
#define modes_unset_mode(modes, newmode) modes_change_mode(modes, FALSE, newmode)
G_MODULE_EXPORT char get_prefix_from_modes(struct network_info *info, irc_modes_t modes);
G_MODULE_EXPORT gboolean mode_is_channel_mode(struct network_info *info, char mode);
G_MODULE_EXPORT char get_mode_by_prefix(char prefix, const struct network_info *n);
G_MODULE_EXPORT char get_prefix_by_mode(char mode, const struct network_info *n);

#endif /* __CTRLPROXY_STATE_H__ */
