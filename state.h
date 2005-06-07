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

struct channel_nick {
	char mode;
	struct channel_state *channel;
	struct network_nick *global_nick;
};

struct network_nick {
	guint refcount;
	char *nick;
	char *fullname;
	char *username;
	char *hostname;
	char *hostmask;
	char modes[255];
	GList *channels;
};

struct banlist_entry {
	char *hostmask;
	char *by;
	time_t time_set;
};

struct channel_state {
	char *name;
	char *key;
	char *topic;
	int joined:1;
	char mode; /* Private, secret, etc */
	char modes[255];
	char introduced;
	int namreply_started:1;
	int banlist_started:1;
	int invitelist_started:1;
	int exceptlist_started:1;
	long limit;
	GList *nicks;
	GList *banlist;
	GList *invitelist;
	GList *exceptlist;
	struct network_state *network;
};


struct network_info
{
	char *name;
	char *server;
	GHashTable *features;
	char *supported_user_modes;
	char *supported_channel_modes;
	enum { 
		CASEMAP_UNKNOWN = 0, 
		CASEMAP_RFC1459, 
		CASEMAP_ASCII, 
		CASEMAP_STRICT_RFC1459 
	} casemapping;
	int channellen;
	int nicklen;
	int topiclen;
};

struct network_state 
{
	GList *channels;
	GList *nicks;
	struct network_nick me;
	struct network_info info;
};

/* state.c */
G_MODULE_EXPORT struct channel_state *find_channel(struct network_state *st, const char *name);
G_MODULE_EXPORT struct channel_nick *find_nick(struct channel_state *c, const char *name);
G_MODULE_EXPORT struct linestack_context *linestack_new_by_network(struct network *);
G_MODULE_EXPORT GSList *gen_replication_network(struct network_state *s);
G_MODULE_EXPORT GSList *gen_replication_channel(struct channel_state *c, const char *hostmask, const char *nick);
G_MODULE_EXPORT int is_channelname(const char *name, struct network_info *s);
G_MODULE_EXPORT int is_prefix(char p, struct network_info *n);
G_MODULE_EXPORT char get_prefix_by_mode(char p, struct network_info *n);
G_MODULE_EXPORT const char *get_network_feature(struct network_info *n, const char *name);
G_MODULE_EXPORT int irccmp(struct network_info *n, const char *a, const char *b);
G_MODULE_EXPORT struct network_nick *line_get_network_nick(struct line *l);

#endif /* __CTRLPROXY_STATE_H__ */
