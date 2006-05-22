/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2003 Jelmer Vernooij <jelmer@nl.linux.org>

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

#include "internals.h"
#include "irc.h"

static void free_network_nick(struct network_state *, struct network_nick *);
static void free_channel(struct channel_state *c);

enum mode_type { REMOVE = 0, ADD = 1};

void network_nick_set_data(struct network_nick *n, const char *nick, const char *username, const char *host)
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
		n->hostmask = g_strdup_printf("%s!~%s@%s", nick, username, host);
	}
}

gboolean network_nick_set_nick(struct network_nick *n, const char *nick)
{
	g_assert(n);

	if (n == NULL)
		return FALSE;

	if (n->nick != NULL && !strcmp(nick, n->nick)) 
		return TRUE;

	g_free(n->nick);
	n->nick = g_strdup(nick);
	
	g_free(n->hostmask);
	n->hostmask = g_strdup_printf("%s!~%s@%s", nick, n->username, n->hostname);

	return TRUE;
}

gboolean network_nick_set_hostmask(struct network_nick *n, const char *hm)
{
	char *t, *u;

	g_assert(n);

	if (n == NULL)
		return FALSE;

	g_assert(hm);

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

	g_free(n);
}

static void free_invitelist(struct channel_state *c)
{
	GList *g;
	g_assert(c);

	g = c->invitelist;
	while(g) {
		g_free(g->data);
		g = g_list_remove(g, g->data);
	}
	c->invitelist = NULL;
}

static void free_exceptlist(struct channel_state *c)
{
	GList *g;
	g_assert(c);

	g = c->exceptlist;
	while(g) {
		g_free(g->data);
		g = g_list_remove(g, g->data);
	}
	c->exceptlist = NULL;
}

static void free_banlist(struct channel_state *c)
{
	GList *g;
	
	g_assert(c);
	g = c->banlist;
	while(g) {
		struct banlist_entry *be = g->data;
		g_free(be->hostmask);
		g_free(be->by);
		g = g_list_remove(g, be);
		g_free(be);
	}
	c->banlist = NULL;
}

static void free_names(struct channel_state *c)
{
	while(c->nicks) {
		free_channel_nick((struct channel_nick *)c->nicks->data);
	}
	c->nicks = NULL;
}

static void free_channel(struct channel_state *c)
{
	if (c == NULL)
		return;
	free_names(c);
	g_free(c->name);
	g_free(c->topic);
	g_free(c->key);
	g_assert(c->network);
	c->network->channels = g_list_remove(c->network->channels, c);
	g_free(c);
}

struct channel_state *find_channel(struct network_state *st, const char *name)
{
	GList *cl;
	g_assert(st);
	g_assert(name);
	for (cl = st->channels; cl; cl = cl->next) {
		struct channel_state *c = (struct channel_state *)cl->data;

		if(!irccmp(st->info, c->name, name)) 
			return c;
	}
	return NULL;
}

struct channel_state *find_add_channel(struct network_state *st, char *name) 
{
	struct channel_state *c;
	g_assert(st);
	g_assert(name);
	
	c = find_channel(st, name);
	if(c)
		return c;
	c = g_new0(struct channel_state ,1);
	c->network = st;
	st->channels = g_list_append(st->channels, c);
	c->name = g_strdup(name);

	return c;
}

struct channel_nick *find_channel_nick(struct channel_state *c, const char *name) 
{
	GList *l;
	const char *realname = name;
	g_assert(name);
	g_assert(c);

	g_assert(c->network);
	if(is_prefix(realname[0], c->network->info))
		realname++;

	for (l = c->nicks; l; l = l->next) {
		struct channel_nick *n = (struct channel_nick *)l->data;
		if(!irccmp(c->network->info, n->global_nick->nick, realname))
			return n;
	}

	return NULL;
}

struct network_nick *find_network_nick(struct network_state *n, const char *name)
{
	GList *gl;

	g_assert(name);
	g_assert(n);

	if (!irccmp(n->info, n->me.nick, name))
		return &n->me;

	for (gl = n->nicks; gl; gl = gl->next) {
		struct network_nick *ndd = (struct network_nick*)gl->data;
		if(!irccmp(n->info, ndd->nick, name)) {
			return ndd;
		}
	}

	return NULL;
}

static struct network_nick *find_add_network_nick(struct network_state *n, const char *name)
{
	struct network_nick *nd;

	g_assert(name);
	g_assert(n);

	nd = find_network_nick(n, name);
	if (nd) 
		return nd;

	/* create one, if it doesn't exist */
	nd = g_new0(struct network_nick,1);
	nd->nick = g_strdup(name);
	
	n->nicks = g_list_append(n->nicks, nd);
	return nd;
}

struct channel_nick *find_add_channel_nick(struct channel_state *c, const char *name) 
{
	struct channel_nick *n;
	const char *realname = name;
	char mymode = 0;

	g_assert(c);
	g_assert(name);
	g_assert(strlen(name) > 0);
	g_assert(c->network);

	if(is_prefix(realname[0], c->network->info)) {
		mymode = realname[0];
		realname++;
	}

	n = find_channel_nick(c, realname);
	if(n) 
		return n;

	n = g_new0(struct channel_nick,1);
	
	n->channel = c;
	n->mode = mymode;
	n->global_nick = find_add_network_nick(c->network, realname);
	c->nicks = g_list_append(c->nicks, n);
	n->global_nick->channel_nicks = g_list_append(n->global_nick->channel_nicks, n);
	return n;
}

static void handle_join(struct network_state *s, struct line *l)
{
	struct channel_state *c;
	struct channel_nick *ni;
	int i;
	char **channels;
	char *nick;

	if ((nick = line_get_nick(l)) == NULL) {
		log_network_state(NULL, LOG_WARNING, s, "No hostmask for JOIN line received from server");
		return;
	}
	
	channels = g_strsplit(l->args[1], ",", 0);

	for (i = 0; channels[i]; i++) {
		/* Someone is joining a channel the user is on */
		c = find_add_channel(s, channels[i]);
		g_assert(s->channels);
		ni = find_add_channel_nick(c, nick);
		network_nick_set_hostmask(ni->global_nick, l->origin);

		/* The user is joining a channel */
		if(!irccmp(s->info, nick, s->me.nick)) {
			log_network_state(NULL, LOG_INFO, s, "Joining channel %s", c->name);
		} else {
			log_network_state(NULL, LOG_TRACE, s, "%s joins channel %s", nick, c->name);
		}
	}
	g_free(nick);
	g_strfreev(channels);
}


static void handle_part(struct network_state *s, struct line *l)
{
	struct channel_state *c;
	struct channel_nick *n;
	char **channels;
	int i;
	char *nick = line_get_nick(l);

	if (!nick)return;

	channels = g_strsplit(l->args[1], ",", 0);

	for(i = 0; channels[i]; i++) {
		c = find_channel(s, channels[i]);

		if(!c){
			log_network_state(NULL, LOG_WARNING, s, "Can't part or let other nick part %s(unknown channel)", channels[i]);
			continue;
		}

		n = find_channel_nick(c, nick);
		if(n) {
			free_channel_nick(n);
		} else {
			log_network_state(NULL, LOG_WARNING, s, "Can't remove nick %s from channel %s: nick not on channel", nick, channels[i]);
		}

		if(!irccmp(s->info, nick, s->me.nick) && c) {
			log_network_state(NULL, LOG_INFO, s, "Leaving %s", channels[i]);
			free_channel(c);
		} else {
			log_network_state(NULL, LOG_TRACE, s, "%s leaves %s", nick, channels[i]);
		}
	}
	g_free(nick);
	g_strfreev(channels);
}

static void handle_kick(struct network_state *s, struct line *l) 
{
	struct channel_state *c;
	struct channel_nick *n;
	char **channels, **nicks;
	int i;
	char *nick = line_get_nick(l);

	channels = g_strsplit(l->args[1], ",", 0);
	nicks = g_strsplit(l->args[2], ",", 0);

	for (i = 0; channels[i] && nicks[i]; i++) {
		c = find_channel(s, channels[i]);

		if(!c){
			log_network_state(NULL, LOG_WARNING, s, "Can't kick nick %s from %s", nicks[i], channels[i]);
			continue;
		}

		n = find_channel_nick(c, nicks[i]);
		if(!n) {
			log_network_state(NULL, LOG_WARNING, s, "Can't kick nick %s from channel %s: nick not on channel", nicks[i], channels[i]);
			continue;
		}

		free_channel_nick(n);

		if(!irccmp(s->info, nick, s->me.nick) && c) {
			log_network_state(NULL, LOG_INFO, s, "Kicked off %s by %s", c->name, nick);
			free_channel(c);
		} else {
			log_network_state(NULL, LOG_TRACE, s, "%s kicked off %s by %s", nicks[i], channels[i], nick);
		}
	}

	if(channels[i] || nicks[i]) {
		log_network_state(NULL, LOG_WARNING, s, "KICK command has unequal number of channels and nicks");
	}

	g_free(nick);
	g_strfreev(nicks);
	g_strfreev(channels);
}

static void handle_topic(struct network_state *s, struct line *l) 
{
	struct channel_state *c = find_channel(s, l->args[1]);
	if(c->topic)g_free(c->topic);
	c->topic = g_strdup(l->args[2]);
}

static void handle_332(struct network_state *s, struct line *l) 
{
	struct channel_state *c = find_channel(s, l->args[2]);

	if(!c) {
		log_network_state(NULL, LOG_WARNING, s, "Can't set topic for unknown channel '%s'!", l->args[2]);
		return;
	}

	if(c->topic)g_free(c->topic);
	c->topic = g_strdup(l->args[3]);
}

static void handle_no_topic(struct network_state *s, struct line *l) 
{
	struct channel_state *c = find_channel(s, l->args[1]);

	if(!c) {
		log_network_state(NULL, LOG_WARNING, s, "Can't unset topic for unknown channel '%s'!", l->args[2]);
		return;
	}

	if(c->topic)g_free(c->topic);
	c->topic = NULL;
}

static void handle_namreply(struct network_state *s, struct line *l) 
{
	gchar **names;
	int i;
	struct channel_state *c = find_channel(s, l->args[3]);
	if(!c) {
		log_network_state(NULL, LOG_WARNING, s, "Can't add names to %s: channel not found", l->args[3]);
		return;
	}
	c->mode = l->args[2][0];
	if(!c->namreply_started) {
		free_names(c);
		c->namreply_started = TRUE;
	}
	names = g_strsplit(l->args[4], " ", -1);
	for (i = 0; names[i]; i++) {
		if (strlen(names[i]) == 0) continue;
		find_add_channel_nick(c, names[i]);
	}
	g_strfreev(names);
}

static void handle_end_names(struct network_state *s, struct line *l) 
{
	struct channel_state *c = find_channel(s, l->args[2]);
	if(c)c->namreply_started = FALSE;
	else log_network_state(NULL, LOG_WARNING, s, "Can't end /NAMES command for %s: channel not found\n", l->args[2]);
}

static void handle_invitelist_entry(struct network_state *s, struct line *l) 
{
	struct channel_state *c = find_channel(s, l->args[2]);
	
	if(!c) {
		log_network_state(NULL, LOG_WARNING, s, "Can't add invitelist entries to %s: channel not found", l->args[2]);
		return;
	}

	if (!c->invitelist_started) {
		free_invitelist(c);
		c->invitelist_started = TRUE;
	}

	c->invitelist = g_list_append(c->invitelist, g_strdup(l->args[3]));
}

static void handle_end_invitelist(struct network_state *s, struct line *l) 
{
	struct channel_state *c = find_channel(s, l->args[2]);
	if(c)c->invitelist_started = FALSE;
	else log_network_state(NULL, LOG_WARNING, s, "Can't end invitelist for %s: channel not found\n", l->args[2]);
}

static void handle_exceptlist_entry(struct network_state *s, struct line *l) 
{
	struct channel_state *c = find_channel(s, l->args[2]);
	
	if(!c) {
		log_network_state(NULL, LOG_WARNING, s, "Can't add exceptlist entries to %s: channel not found", l->args[2]);
		return;
	}

	if (!c->exceptlist_started) {
		free_exceptlist(c);
		c->exceptlist_started = TRUE;
	}

	c->exceptlist = g_list_append(c->exceptlist, g_strdup(l->args[3]));
}

static void handle_end_exceptlist(struct network_state *s, struct line *l) 
{
	struct channel_state *c = find_channel(s, l->args[2]);
	if(c)c->exceptlist_started = FALSE;
	else log_network_state(NULL, LOG_WARNING, s, "Can't end exceptlist for %s: channel not found\n", l->args[2]);
}



static void handle_banlist_entry(struct network_state *s, struct line *l) 
{
	struct channel_state *c = find_channel(s, l->args[2]);
	struct banlist_entry *be;
	
	if(!c) {
		log_network_state(NULL, LOG_WARNING, s, "Can't add banlist entries to %s: channel not found", l->args[2]);
		return;
	}

	if (!c->banlist_started) {
		free_banlist(c);
		c->banlist_started = TRUE;
	}

	be = g_new0(struct banlist_entry, 1);
	be->hostmask = g_strdup(l->args[3]);
	if (l->args[4]) {
		be->by = g_strdup(l->args[4]);
		if (l->args[5]) 
			be->time_set = atol(l->args[5]);
	}

	c->banlist = g_list_append(c->banlist, be);
}

static void handle_end_banlist(struct network_state *s, struct line *l) 
{
	struct channel_state *c = find_channel(s, l->args[2]);
	if(c)c->banlist_started = FALSE;
	else log_network_state(NULL, LOG_WARNING, s, "Can't end banlist for %s: channel not found\n", l->args[2]);
}

static void handle_whoreply(struct network_state *s, struct line *l) 
{
	struct channel_nick *n; struct channel_state *c;

	c = find_channel(s, l->args[2]);
	if(!c) 
		return;

	n = find_add_channel_nick(c, l->args[6]);
	network_nick_set_data(n->global_nick, l->args[6], l->args[3], l->args[4]);
}

static void handle_end_who(struct network_state *s, struct line *l) 
{
}

static void handle_quit(struct network_state *s, struct line *l) 
{
	char *nick = line_get_nick(l);
	struct network_nick *nn = find_network_nick(s, nick);
	g_free(nick);

	g_assert(nn != &s->me);

	if (nn) 
		free_network_nick(s, nn);
}

static void handle_mode(struct network_state *s, struct line *l)
{
	/* Format:
	 * MODE %|<nick>|<channel> [<mode> [<mode parameters>]] */
	enum mode_type t = ADD;
	int i;

	/* Channel modes */
	if(is_channelname(l->args[1], s->info)) {
		struct channel_state *c = find_channel(s, l->args[1]);
		struct channel_nick *n;
		char p;
		int arg = 2;

		if (c == NULL) {
			log_network_state(NULL, LOG_WARNING, s, "Unable to change mode for unknown channel '%s'\n", l->args[1]);
			return;
		}
		
		for(i = 0; l->args[2][i]; i++) {
			switch(l->args[2][i]) {
				case '+': t = ADD; break;
				case '-': t = REMOVE; break;
				case 'b': /* Ban */
						  {
							  struct banlist_entry *be = g_new0(struct banlist_entry, 1);
							  be->time_set = time(NULL);
							  be->hostmask = g_strdup(l->args[arg]);
							  be->by = line_get_nick(l);
							  c->banlist = g_list_append(c->banlist, be);
						  }
												
						  arg++;
						  break;
				case 'l':
					c->modes[(unsigned char)'l'] = t;
				    if (t) {
						if(!l->args[++arg]) {
							log_network_state(NULL, LOG_WARNING, s, "Mode +l requires argument, but no argument found");
							break;
						}
						c->limit = atol(l->args[arg]);
					} else {
						c->limit = 0;
					}
					break;
				case 'k':
					c->modes[(unsigned char)'k'] = t;
					if (t) {
						if(!l->args[++arg]) {
							log_network_state(NULL, LOG_WARNING, s, "Mode k requires argument, but no argument found");
							break;
						}

						g_free(c->key);
						c->key = g_strdup(l->args[arg]);
					} else {
						g_free(c->key);
						c->key = NULL;
					}

					break;
				default:
					  p = get_prefix_by_mode(l->args[2][i], s->info);
					  if(p == ' ') {
						  c->modes[(unsigned char)l->args[2][i]] = t;
					  } else {
							n = find_channel_nick(c, l->args[++arg]);
							if(!n) {
								log_network_state(NULL, LOG_WARNING, s, "Can't set mode %c%c on nick %s on channel %s, because nick does not exist!", t == ADD?'+':'-', l->args[2][i], l->args[arg], l->args[1]);
								break;
							}
							n->mode = (t == ADD?p:' ');
					  }
					  break;
			}
		}
		/* User modes */
	} else {
		struct network_nick *nn = find_add_network_nick(s, l->args[1]);

		for(i = 0; l->args[2][i]; i++) {
			switch(l->args[2][i]) {
				case '+': t = ADD;break;
				case '-': t = REMOVE; break;
				default:
					  nn->modes[(unsigned char)l->args[2][i]] = t;
					  break;
			}
		}
	}
}

static void handle_001(struct network_state *s, struct line *l)
{
	g_free(s->me.nick);
	s->me.nick = g_strdup(l->args[1]);
}

static void handle_004(struct network_state *s, struct line *l)
{
	s->info->supported_user_modes = g_strdup(l->args[4]);
	s->info->supported_channel_modes = g_strdup(l->args[5]);
	s->info->server = g_strdup(l->args[2]);
}

static void handle_privmsg(struct network_state *s, struct line *l)
{
	struct network_nick *nn;
	char *nick;
	if (irccmp(s->info, l->args[1], s->me.nick) != 0) return;

	nick = line_get_nick(l);
	nn = find_add_network_nick(s, nick);
	nn->query = 1;
	g_free(nick);
}

static void handle_nick(struct network_state *s, struct line *l)
{
	struct network_nick *nn;
	char *nick = line_get_nick(l);
	nn = find_add_network_nick(s, nick);
	g_free(nick);
	network_nick_set_nick(nn, l->args[1]);
}

static void handle_umodeis(struct network_state *s, struct line *l)
{
	int i;
	memset(s->me.modes, 0, sizeof(s->me.modes));
	for (i = 0; i < strlen(l->args[1]); i++) {
		s->me.modes[(unsigned char)l->args[1][i]] = 1;
	}
}

static void handle_302(struct network_state *s, struct line *l)
{
	/* We got a USERHOST response, split it into nick and user@host, and check the nick */
	gchar** tmp302 = g_strsplit(g_strstrip(l->args[2]), "=+", 2);
	if (g_strv_length(tmp302) > 1) {
		char *hm;
		struct network_nick *nn = find_add_network_nick(s, tmp302[0]);
		
		hm = g_strdup_printf("%s!%s", tmp302[0], tmp302[1]);
		network_nick_set_hostmask(nn, hm);
		g_free(hm);
	}
	g_strfreev(tmp302);
}

extern void handle_005(struct network_state *s, struct line *l);

static struct irc_command {
	char *command;
	int min_args;
	void (*handler) (struct network_state *s, struct line *l);
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
	{ "332", 3, handle_332 },
	{ "331", 1, handle_no_topic },
	{ "353", 4, handle_namreply },
	{ "366", 2, handle_end_names },
	{ "367", 2, handle_banlist_entry },
	{ "368", 2, handle_end_banlist },
	{ "346", 2, handle_invitelist_entry },
	{ "347", 2, handle_end_invitelist },
	{ "348", 2, handle_exceptlist_entry },
	{ "349", 2, handle_end_exceptlist },
	{ "352", 7, handle_whoreply },
	{ "315", 1, handle_end_who },
	{ NULL }
};

gboolean state_handle_data(struct network_state *s, struct line *l)
{
	int i,j;

	if(!s || !l->args || !l->args[0])
		return FALSE;

	for(i = 0; irc_commands[i].command; i++) {
		if(!g_strcasecmp(irc_commands[i].command, l->args[0])) {
			for(j = 0; j <= irc_commands[i].min_args; j++) {
				if(!l->args[j])
					return FALSE;
			}
			irc_commands[i].handler(s,l);
			return TRUE;
		}
	}

	return FALSE;
}

struct network_state *network_state_init(struct network_info *info, const char *nick, const char *username, const char *hostname)
{
	struct network_state *state = g_new0(struct network_state, 1);
	state->info = info;
	state->me.query = 1;
	network_nick_set_data(&state->me, nick, username, hostname);

	return state;
}

void free_network_nick(struct network_state *st, struct network_nick *nn)
{
	g_assert(nn != &st->me);
	g_assert(nn);

	/* No recursion please... */
	nn->query = 1;

	while(nn->channel_nicks) {
		struct channel_nick *n = nn->channel_nicks->data;
		free_channel_nick(n);
	}

	g_free(nn->hostmask);
	g_free(nn->username);
	g_free(nn->hostname);
	g_free(nn->nick);
	st->nicks = g_list_remove(st->nicks, nn);
	g_free(nn);
}

void free_network_state(struct network_state *state)
{
	if (state == NULL)
		return;

	while(state->channels)
		free_channel((struct channel_state *)state->channels->data);

	g_free(state->me.nick);
	g_free(state->me.username);
	g_free(state->me.hostname);
	g_free(state->me.hostmask);

	while(state->nicks) 
	{
		struct network_nick *nn = state->nicks->data;
		free_network_nick(state, nn);
	}

	g_free(state);
}

/*
 * Marshall/unmarshall functions
 */

enum marshall_mode { MARSHALL_PULL = 0, MARSHALL_PUSH = 1 };

struct data_blob {
	char *data;
	size_t offset;
	size_t length;
};

typedef gboolean (*marshall_fn_t) (struct network_state *, enum marshall_mode, struct data_blob *db, void **data);

static void blob_increase_size (struct data_blob *db, size_t size)
{
	if (db->offset + size > db->length) {
		db->length += 1000;
		db->data = g_realloc(db->data, db->length);
	}
}

#define marshall_new(m,t) if ((m) == MARSHALL_PULL) *(t) = g_malloc(sizeof(**t));
#define marshall_type(nst,m,db,n) marshall_bytes(nst,m,db,(void *)(n),sizeof(*(n)))

static gboolean marshall_bytes (struct network_state *nst, enum marshall_mode m, struct data_blob *t, void *d, size_t length)
{
	if (m == MARSHALL_PULL) {
		if (t->offset + sizeof(size_t) > t->length)
			return FALSE;

		memcpy(&length, t->data+t->offset, sizeof(size_t));
		t->offset += sizeof(size_t);

		memcpy(d, t->data+t->offset, length);

		t->offset += length;
	} else {
		blob_increase_size(t, sizeof(size_t) + length);

		memcpy(t->data+t->offset, &length, sizeof(size_t));
		t->offset += sizeof(size_t);

		memcpy(t->data+t->offset, d, length);

		t->offset += length;
	}

	return TRUE;
}

static gboolean marshall_string (struct network_state *nst, enum marshall_mode m, struct data_blob *t, char **d)
{
	size_t length = -1;

	if (m == MARSHALL_PUSH && *d != NULL) 
		length = strlen(*d);

	marshall_type(nst, m, t, &length);

	if (length == -1) {
		*d = NULL;
		return TRUE;
	} else if (m == MARSHALL_PULL) {
		*d = g_new0(char, length+1);
	}
	
	return marshall_bytes(nst, m, t, *d, length);
}



static gboolean marshall_network_nick (struct network_state *nst, enum marshall_mode m, struct data_blob *t, struct network_nick *n)
{
	gboolean ret = TRUE;
	ret &= marshall_type(nst, m, t, &n->query);
	ret &= marshall_type(nst, m, t, n->modes);
	ret &= marshall_string(nst, m, t, &n->nick);
	g_assert(n->nick);
	ret &= marshall_string(nst, m, t, &n->fullname);
	ret &= marshall_string(nst, m, t, &n->username);
	ret &= marshall_string(nst, m, t, &n->hostname);
	ret &= marshall_string(nst, m, t, &n->hostmask);
	if (m == MARSHALL_PULL)
		n->channel_nicks = NULL;
	return ret;
}


static gboolean marshall_network_nick_p (struct network_state *nst, enum marshall_mode m, struct data_blob *t, struct network_nick **n)
{
	marshall_new(m, n);
	return marshall_network_nick(nst,m,t,*n);
}

struct ht_traverse_data 
{
	marshall_fn_t key_fn;
	marshall_fn_t val_fn;
	struct data_blob *data;
	struct network_state *nst;
};

static gboolean marshall_GList (struct network_state *nst, enum marshall_mode m, struct data_blob *t, GList **gl, marshall_fn_t marshall_fn)
{
	if (m == MARSHALL_PULL) {
		size_t count;
		int i;

		marshall_type(nst, m, t, &count);

		*gl = NULL;

		for (i = 0; i < count; i++) {
			void *v;
			if (!marshall_fn(nst, m, t, &v))
				return FALSE;
			*gl = g_list_append(*gl, v);
		}
	} else {
		size_t count = g_list_length(*gl);
		GList *l;

		marshall_type(nst, m, t, &count);

		for (l = *gl; l; l = l->next) {
			if (!marshall_fn(nst, m, t, &l->data))
				return FALSE;
		}
	}

	return TRUE;
}

static gboolean marshall_banlist_entry (struct network_state *nst, enum marshall_mode m, struct data_blob *t, struct banlist_entry **d)
{
	gboolean ret = TRUE;
	marshall_new(m, d);
	ret &= marshall_type(nst, m, t, &(*d)->time_set);
	ret &= marshall_string(nst, m, t, &(*d)->hostmask);
	ret &= marshall_string(nst, m, t, &(*d)->by);
	return ret;
}

static gboolean marshall_channel_state (struct network_state *nst, enum marshall_mode m, struct data_blob *t, struct channel_state **c)
{
	gboolean ret = TRUE;
	marshall_new(t, c);

	ret &= marshall_type(nst, m, t, &(*c)->mode);
	ret &= marshall_type(nst, m, t, (*c)->modes);
	ret &= marshall_type(nst, m, t, &(*c)->namreply_started);
	ret &= marshall_type(nst, m, t, &(*c)->banlist_started);
	ret &= marshall_type(nst, m, t, &(*c)->invitelist_started);
	ret &= marshall_type(nst, m, t, &(*c)->exceptlist_started);
	ret &= marshall_type(nst, m, t, &(*c)->limit);
	ret &= marshall_string(nst, m, t, &(*c)->name);
	ret &= marshall_string(nst, m, t, &(*c)->key);
	ret &= marshall_string(nst, m, t, &(*c)->topic);
	ret &= marshall_GList(nst, m, t, &(*c)->banlist, (marshall_fn_t)marshall_banlist_entry);
	ret &= marshall_GList(nst, m, t, &(*c)->invitelist, (marshall_fn_t)marshall_string);
	ret &= marshall_GList(nst, m, t, &(*c)->exceptlist, (marshall_fn_t)marshall_string);
	(*c)->network = nst;

	/* Nicks  */
	if (m == MARSHALL_PULL) {
		size_t count;
		int i;

		marshall_type(nst, m, t, &count);

		for (i = 0; i < count; i++) {
			struct channel_nick *cn;
			char *nick;
			
			if (!marshall_string(nst, m, t, &nick))
				return FALSE;
			
			cn = find_add_channel_nick(*c, nick);
			g_assert(cn);

			if (!marshall_type(nst, m, t, &cn->mode))
				return FALSE;

			g_free(nick);
		}
	} else {
		size_t count = g_list_length((*c)->nicks);
		GList *l;

		marshall_type(nst, m, t, &count);

		for (l = (*c)->nicks; l; l = l->next) {
			struct channel_nick *cn = l->data;
			if (!marshall_string(nst, m, t, &cn->global_nick->nick))
				return FALSE;

			if (!marshall_type(nst, m, t, &cn->mode))
				return FALSE;
		}
	}

	return ret;
}

static gboolean marshall_network_state (struct network_state *nst, enum marshall_mode m, struct data_blob *t, struct network_state *n)
{
	gboolean ret = TRUE;

	ret &= marshall_GList(n, m, t, &n->nicks, (marshall_fn_t)marshall_network_nick_p);
	ret &= marshall_GList(n, m, t, &n->channels, (marshall_fn_t)marshall_channel_state);
	ret &= marshall_network_nick(n, m, t, &n->me);

	g_assert(n->me.nick);

	return ret;
}


struct network_state *network_state_decode(char *blob, size_t len, struct network_info *info)
{
	struct network_state *ret;
	struct data_blob db;

	if (len == 0)
		return NULL;
	
	db.data = blob;
	db.offset = 0;
	db.length = len;

	ret = g_new0(struct network_state, 1);
	ret->info = info;
	if (!marshall_network_state(NULL, MARSHALL_PULL, &db, ret)) 
		return NULL;

	return ret;
}

char *network_state_encode(const struct network_state *st, size_t *len)
{
	struct data_blob db;
	db.data = NULL;
	db.offset = 0;
	db.length = 0;
	*len = 0;

	if (st == NULL)
		return NULL;
	if (!marshall_network_state(st, MARSHALL_PUSH, &db, st))
		return NULL;
	
	*len = db.offset;
	return db.data;
}

struct network_state *network_state_dup(struct network_state *orig)
{
	size_t len;
	struct network_state *ret;
	char *data = network_state_encode(orig, &len);

	if (data == NULL)
		return NULL;

	ret = network_state_decode(data, len, orig->info);

	g_free(data);

	return ret;
}
