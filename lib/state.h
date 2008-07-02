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

#define MAXMODES 255
typedef gboolean irc_modes_t[MAXMODES];

/**
 * @file
 * @brief State information
 */

struct irc_network;
struct irc_client;
struct irc_line;

/* When changing one of these structs, also change the marshalling
 * function for that struct in state.c */

/**
 * Record of a nick on a channel.
 */
struct channel_nick {
	irc_modes_t modes;
	struct network_nick *global_nick;
	struct irc_channel_state *channel;

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
 * An entry in the nicklist of a channel.
 */
struct nicklist_entry {
	char *hostmask;
	char *by;
	time_t time_set;
};

struct nicklist_entry *find_nicklist_entry(GList *entries, const char *hostmask);
void free_nicklist_entry(struct nicklist_entry *be);
void free_nicklist(GList **nicklist);
gboolean nicklist_add_entry(GList **nicklist, const char *opt_arg,
								   const char *by_nick, time_t at);
gboolean nicklist_remove_entry(GList **nicklist, const char *hostmask);
#define channel_mode_nicklist(ch,mode) ((ch)->chanmode_nicklist[(unsigned char)mode])
#define channel_mode_option(ch,mode) ((ch)->chanmode_option[(unsigned char)mode])

/**
 * The state of a particular channel.
 */
struct irc_channel_state {
	char *name;
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
	GList *nicks;

	struct irc_network_state *network;

	GList *chanmode_nicklist[MAXMODES];
	char *chanmode_option[MAXMODES];
};

/**
 * Describes the (partial) state of a network at a specific time
 */
struct irc_network_state 
{
	/** Private data to be used by whatever code is using network_state. */
	void *userdata;
	/** Function to use for logging changes and abnormalities. */
	void (*log) (enum log_level l,
				 void *userdata,
				 const char *msg);
	/** List of known channels. */
	GList *channels;
	/** List of known nicks. */
	GList *nicks;
	/** Information for the user itself. */
	struct network_nick me;
	/** Network static info. */
	struct irc_network_info *info;
	/** Whether or not the user is currently away. */
	gboolean is_away;
};

/* state.c */
G_GNUC_MALLOC G_MODULE_EXPORT struct irc_network_state *network_state_init(const char *nick, const char *username, const char *hostname);
G_MODULE_EXPORT void free_network_state(struct irc_network_state *);
G_MODULE_EXPORT gboolean state_handle_data(struct irc_network_state *s, const struct irc_line *l);

G_MODULE_EXPORT struct irc_channel_state *find_channel(struct irc_network_state *st, const char *name);
G_MODULE_EXPORT struct channel_nick *find_channel_nick(struct irc_channel_state *c, const char *name);
G_MODULE_EXPORT struct channel_nick *find_channel_nick_hostmask(struct irc_channel_state *c, const char *hostmask);
G_MODULE_EXPORT struct channel_nick *find_add_channel_nick(struct irc_channel_state *c, const char *name);
G_MODULE_EXPORT struct network_nick *find_network_nick(struct irc_network_state *c, const char *name);
G_MODULE_EXPORT gboolean network_nick_set_hostmask(struct network_nick *n, const char *hm);
G_MODULE_EXPORT gboolean client_send_state(struct irc_client *, struct irc_network_state *);
G_MODULE_EXPORT void network_state_log(enum log_level l, const struct irc_network_state *st, const char *fmt, ...);
G_MODULE_EXPORT void network_state_set_log_fn(struct irc_network_state *st, void (*fn) (enum log_level, void *, const char *), void *userdata);

G_MODULE_EXPORT G_GNUC_MALLOC char *mode2string(irc_modes_t modes);
G_MODULE_EXPORT gboolean string2mode(const char *modestring, irc_modes_t modes);
G_MODULE_EXPORT gboolean modes_change_mode(irc_modes_t modes, gboolean set, char newmode);
#define modes_set_mode(modes, newmode) modes_change_mode(modes, TRUE, newmode)
#define modes_unset_mode(modes, newmode) modes_change_mode(modes, FALSE, newmode)
#define modes_clear(modes) memset(modes, 0, sizeof(gboolean) * MAXMODES)
#define modes_cmp(a,b) memcmp(a,b,sizeof(gboolean) * MAXMODES)
G_MODULE_EXPORT char get_prefix_from_modes(struct irc_network_info *info, irc_modes_t modes);
G_MODULE_EXPORT gboolean is_channel_mode(struct irc_network_info *info, char mode);
G_MODULE_EXPORT gboolean is_user_mode(struct irc_network_info *info, char mode);
G_MODULE_EXPORT char get_mode_by_prefix(char prefix, const struct irc_network_info *n);
G_MODULE_EXPORT char get_prefix_by_mode(char mode, const struct irc_network_info *n);
G_MODULE_EXPORT gboolean is_prefix_mode(const struct irc_network_info *info, char mode);

G_MODULE_EXPORT void free_channel_state(struct irc_channel_state *c);
G_MODULE_EXPORT struct irc_channel_state *irc_channel_state_new(const char *name);

#endif /* __CTRLPROXY_STATE_H__ */
