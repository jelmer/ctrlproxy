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
	time_t topic_set_time;
	char *topic_set_by; /* nickname */
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

	/* Case mapping used for nick- and channel name comparing */
	enum casemapping casemapping;

	/* Maximum channel name length */
	int channellen;

	/* The max length of an away message */
	int awaylen;

	/* Maximum kick comment length */
	int kicklen;

	/* Maximum targets allowed for PRIVMSG and NOTICE commands */
	int maxtargets;

	/* Maximum nickname length */
	int nicklen;

	/* Maximum number of channels allowed to join */
	int maxchannels;

	/* Maximum topic length */ 
	int topiclen;

	/* Maximum number of bans per channel */
	int maxbans;

	/* Maximum number of channel modes with parameter allowed per MODE 
	 * command  */
	int maxmodes;

	/* The server supports messaging channel operators (NOTICE @#channel) */
	gboolean wallchops;

	/* Notice to +#channel goes to all voiced persons */
	gboolean wallvoices;

	/* Server supports RFC 2812 features */
	gboolean rfc2812;

	/* Server gives extra penalty to some commands instead of the normal 2 
	 * seconds per message and 1 second for every 120 bytes in a message */
	gboolean penalty;

	/* Forced nick changes: The server may change the nickname without the 
	 * client sending a NICK message */
	gboolean forced_nick_changes;

	/* The LIST is sent in multiple iterations so send queue won't fill and 
	 * kill the client connection. */
	gboolean safelist;

	/* The USERIP command exists */
	gboolean userip;

	/* The CPRIVMSG command exists, used for mass messaging people in specified 
	 * channel (CPRIVMSG channel nick,nick2,... :text) */
	gboolean cprivmsg;

	/* The CNOTICE command exists, just like CPRIVMSG */
	gboolean cnotice;

	/* The KNOCK command exists */
	gboolean knock;

	/* Server supports virtual channels. See  vchans.txt for more information */
	gboolean vchannels;

	/* The WHO command uses WHOX protocol */
	gboolean whox;

	/* The server supports server side ignores via the +g user mode */
	gboolean callerid;

	/* [Deprecated] The same as CALLERID */
	gboolean accept;
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
G_MODULE_EXPORT void network_info_parse(struct network_info *info, const char *parameter);

#endif /* __CTRLPROXY_STATE_H__ */
