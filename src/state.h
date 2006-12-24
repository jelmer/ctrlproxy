/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2005 Jelmer Vernooij <jelmer@nl.linux.org>

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

#ifndef __CTRLPROXY_STATE_H__
#define __CTRLPROXY_STATE_H__

#include "log.h"

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
	char mode;
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
	char modes[255];
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
	char mode; /* Private, secret, etc */
	char modes[255];
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

enum casemapping { 
		CASEMAP_UNKNOWN = 0, 
		CASEMAP_RFC1459, 
		CASEMAP_ASCII, 
		CASEMAP_STRICT_RFC1459 
	};

/**
 * Information about a network (doesn't change between connects or 
 * servers).
 */
struct network_info
{
	char *name;
	char *server;
	GHashTable *features;
	char *supported_user_modes;
	char *supported_channel_modes;
	enum casemapping casemapping;
	int channellen;
	int nicklen;
	int topiclen;
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
	struct network_info *info;
};

/* state.c */
G_MODULE_EXPORT struct network_state *network_state_init(struct network_info *info, const char *nick, const char *username, const char *hostname);
G_MODULE_EXPORT void free_network_state(struct network_state *);
G_MODULE_EXPORT gboolean state_handle_data(struct network_state *s, struct line *l);

G_MODULE_EXPORT struct channel_state *find_channel(struct network_state *st, const char *name);
G_MODULE_EXPORT struct channel_nick *find_channel_nick(struct channel_state *c, const char *name);
G_MODULE_EXPORT struct channel_nick *find_add_channel_nick(struct channel_state *c, const char *name);
G_MODULE_EXPORT struct network_nick *find_network_nick(struct network_state *c, const char *name);
G_MODULE_EXPORT gboolean network_nick_set_hostmask(struct network_nick *n, const char *hm);
G_MODULE_EXPORT gboolean client_send_state(struct client *, struct network_state *);
G_MODULE_EXPORT gboolean is_channelname(const char *name, const struct network_info *s);
G_MODULE_EXPORT gboolean is_prefix(char p, const struct network_info *n);
G_MODULE_EXPORT char get_prefix_by_mode(char p, const struct network_info *n);
G_MODULE_EXPORT int irccmp(const struct network_info *n, const char *a, const char *b);
G_MODULE_EXPORT gboolean network_supports(const struct network_info *n, const char *fe);
G_MODULE_EXPORT const char *get_charset(const struct network_info *n);

#endif /* __CTRLPROXY_STATE_H__ */
