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

void log_network_state(struct network_state *st, const char *fmt, ...)
{
	/* FIXME */
}

enum mode_type { REMOVE = 0, ADD = 1};

void network_nick_set_data(struct network_nick *n, const char *nick, const char *username, const char *host)
{
	gboolean changed = FALSE;
	
	if (!n->nick || strcmp(nick, n->nick)) {
		g_free(n->nick); n->nick = g_strdup(nick);
		changed = TRUE;
	}

	if (!n->username || strcmp(username, n->username)) {
		g_free(n->username); n->username = g_strdup(username);
		changed = TRUE;
	}
	
	if (!n->hostname || strcmp(host, n->hostname)) {
		g_free(n->hostname); n->hostname = g_strdup(host);
		changed = TRUE;
	}
	
	if (changed) {
		g_free(n->hostmask);
		n->hostmask = g_strdup_printf("%s!~%s@%s", nick, username, host);
	}
}

void network_nick_set_nick(struct network_nick *n, const char *nick)
{
	if (!strcmp(nick, n->nick)) return;

	g_free(n->nick);
	n->nick = g_strdup(nick);
	
	g_free(n->hostmask);
	n->hostmask = g_strdup_printf("%s!~%s@%s", nick, n->username, n->hostname);
}

void network_nick_set_hostmask(struct network_nick *n, const char *hm)
{
	char *t, *u;
	if (n->hostmask && !strcmp(n->hostmask, hm))
		return;

	g_free(n->hostmask);
	g_free(n->nick); n->nick = NULL;
	g_free(n->username); n->username = NULL;
	g_free(n->hostname); n->hostname = NULL;
	n->hostmask = g_strdup(hm);

	t = strchr(hm, '!');
	if (!t) return;
	n->nick = g_strndup(hm, t-hm);
	
	u = strchr(t, '@');
	if (!u) return;
	n->username = g_strndup(t+1, u-t-1);

	n->hostname = g_strdup(u+1);
}

static void free_channel_nick(struct channel_nick *n)
{
	g_assert(n);

	n->channel->nicks = g_list_remove(n->channel->nicks, n);
	n->global_nick->channel_nicks = g_list_remove(n->global_nick->channel_nicks, n);

	if (g_list_length(n->global_nick->channel_nicks) == 0 && n->global_nick->query == 0) 
		free_network_nick(n->channel->network, n->global_nick);

	g_free(n);
}

static void free_invitelist(struct channel_state *c)
{
	GList *g = c->invitelist;
	while(g) {
		g_free(g->data);
		g = g_list_remove(g, g->data);
	}
	c->invitelist = NULL;
}

static void free_exceptlist(struct channel_state *c)
{
	GList *g = c->exceptlist;
	while(g) {
		g_free(g->data);
		g = g_list_remove(g, g->data);
	}
	c->exceptlist = NULL;
}

static void free_banlist(struct channel_state *c)
{
	GList *g = c->banlist;
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
	free_names(c);
	g_free(c->name);
	g_free(c->topic);
	g_free(c->key);
	c->network->channels = g_list_remove(c->network->channels, c);
	g_free(c);
}

struct channel_state *find_channel(struct network_state *st, const char *name)
{
	GList *cl = st->channels;
	while(cl) {
		struct channel_state *c = (struct channel_state *)cl->data;
		if(!irccmp(&st->info, c->name, name)) return c;
		cl = g_list_next(cl);
	}
	return NULL;
}

struct channel_state  *find_add_channel(struct network_state *st, char *name) {
	struct channel_state *c = find_channel(st, name);
	if(c)return c;
	c = g_new0(struct channel_state ,1);
	c->network = st;
	st->channels = g_list_append(st->channels, c);
	c->name = g_strdup(name);

	return c;
}

struct channel_nick *find_channel_nick(struct channel_state *c, const char *name) {
	GList *l;
	const char *realname = name;
	g_assert(name);

	if(is_prefix(realname[0], &c->network->info))realname++;

	for (l = c->nicks; l; l = l->next) {
		struct channel_nick *n = (struct channel_nick *)l->data;
		if(!irccmp(&c->network->info, n->global_nick->nick, realname))
			return n;
	}

	return NULL;
}

struct network_nick *find_network_nick(struct network_state *n, const char *name)
{
	GList *gl;

	g_assert(name);

	for (gl = n->nicks; gl; gl = gl->next) {
		struct network_nick *ndd = (struct network_nick*)gl->data;
		if(!irccmp(&n->info, ndd->nick, name)) {
			return ndd;
		}
	}

	return NULL;
}

static struct network_nick *find_add_network_nick(struct network_state *n, const char *name)
{
	struct network_nick *nd;

	g_assert(name);

	nd = find_network_nick(n, name);
	if (nd) return nd;

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

	g_assert(name);

	g_assert(strlen(name) > 0);

	if(is_prefix(realname[0], &c->network->info)) {
		mymode = realname[0];
		realname++;
	}

	n = find_channel_nick(c, realname);
	if(n) return n;

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
	int cont = 1;
	char *name = g_strdup(l->args[1]), *p, *n;

	p = name;

	while(cont) {
		n = strchr(p, ',');

		if(!n) cont = 0;
		else *n = '\0';

		/* Someone is joining a channel the user is on */
		if(line_get_nick(l)) {
			c = find_add_channel(s, p);
			ni = find_add_channel_nick(c, line_get_nick(l));
			network_nick_set_hostmask(ni->global_nick, l->origin);

			/* The user is joining a channel */
			if(!irccmp(&s->info, line_get_nick(l), s->me.nick)) {
				log_network_state(s, "Joining channel %s", c->name);
			}
		}
		p = n+1;
	}
	g_free(name);
}


static void handle_part(struct network_state *s, struct line *l)
{
	struct channel_state *c;
	struct channel_nick *n;
	int cont = 1;
	char *name = g_strdup(l->args[1]), *p, *m;

	p = name;
	if(!line_get_nick(l))return;

	for(cont = 1; cont; p = m + 1) {
		m = strchr(p, ',');
		if(!m) cont = 0;
		else *m = '\0';

		c = find_channel(s, p);

		if(!c){
			log_network_state(s, "Can't part or let other nick part %s(unknown channel)", p);
			continue;
		}

		n = find_channel_nick(c, line_get_nick(l));
		if(n) {
			free_channel_nick(n);
		} else {
			log_network_state(s, "Can't remove nick %s from channel %s: nick not on channel", line_get_nick(l), p);
		}

		if(!irccmp(&s->info, line_get_nick(l), s->me.nick) && c) {
			log_network_state(s, "Leaving %s", p);
			free_channel(c);
		}
	}
	g_free(name);
}

static void handle_kick(struct network_state *s, struct line *l) 
{
	struct channel_state *c;
	struct channel_nick *n;
	char *nicks = g_strdup(l->args[2]);
	char *channels = g_strdup(l->args[1]);
	char *curnick, *curchan, *nextchan, *nextnick;
	char cont = 1;

	curchan = channels; curnick = nicks;

	while(cont) {
		nextnick = strchr(curnick, ',');
		if(nextnick){ *nextnick = '\0'; nextnick++; }

		nextchan = strchr(curchan, ',');
		if(nextchan){ *nextchan = '\0'; nextchan++; }

		if((!nextnick && nextchan) || (nextnick && !nextchan)) {
			log_network_state(s, "KICK command has unequal number of channels and nicks");
		}

		if(nextnick && nextchan) cont = 1;
		else cont = 0;

		c = find_channel(s, curchan);

		if(!c){
			log_network_state(s, "Can't kick nick %s from %s", curnick, curchan);
			curchan = nextchan; curnick = nextnick;
			continue;
		}

		n = find_channel_nick(c, curnick);
		if(!n) {
			log_network_state(s, "Can't kick nick %s from channel %s: nick not on channel", curnick, curchan);
			curchan = nextchan; curnick = nextnick;
			continue;
		}

		free_channel_nick(n);

		if(!irccmp(&s->info, line_get_nick(l), s->me.nick) && c) {
			log_network_state(s, "Kicked off %s", c->name);
			free_channel(c);
		}
		curchan = nextchan; curnick = nextnick;
	}
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
		log_network_state(s, "Can't set topic for unknown channel '%s'!", l->args[2]);
		return;
	}

	if(c->topic)g_free(c->topic);
	c->topic = g_strdup(l->args[3]);
}

static void handle_no_topic(struct network_state *s, struct line *l) 
{
	struct channel_state *c = find_channel(s, l->args[1]);

	if(!c) {
		log_network_state(s, "Can't unset topic for unknown channel '%s'!", l->args[2]);
		return;
	}

	if(c->topic)g_free(c->topic);
	c->topic = NULL;
}

static void handle_namreply(struct network_state *s, struct line *l) 
{
	char *names, *tmp, *t;
	struct channel_state *c = find_channel(s, l->args[3]);
	if(!c) {
		log_network_state(s, "Can't add names to %s: channel not found", l->args[3]);
		return;
	}
	c->mode = l->args[2][0];
	if(!c->namreply_started) {
		free_names(c);
		c->namreply_started = TRUE;
	}
	tmp = names = g_strdup(l->args[4]);
	while((t = strchr(tmp, ' '))) {
		*t = '\0';
		if(tmp[0])find_add_channel_nick(c, tmp);
		tmp = t+1;
	}
	if(tmp[0])find_add_channel_nick(c, tmp);
	g_free(names);
}

static void handle_end_names(struct network_state *s, struct line *l) 
{
	struct channel_state *c = find_channel(s, l->args[2]);
	if(c)c->namreply_started = FALSE;
	else log_network_state(s, "Can't end /NAMES command for %s: channel not found\n", l->args[2]);
}

static void handle_invitelist_entry(struct network_state *s, struct line *l) 
{
	struct channel_state *c = find_channel(s, l->args[2]);
	
	if(!c) {
		log_network_state(s, "Can't add invitelist entries to %s: channel not found", l->args[2]);
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
	else log_network_state(s, "Can't end invitelist for %s: channel not found\n", l->args[2]);
}

static void handle_exceptlist_entry(struct network_state *s, struct line *l) 
{
	struct channel_state *c = find_channel(s, l->args[2]);
	
	if(!c) {
		log_network_state(s, "Can't add exceptlist entries to %s: channel not found", l->args[2]);
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
	else log_network_state(s, "Can't end exceptlist for %s: channel not found\n", l->args[2]);
}



static void handle_banlist_entry(struct network_state *s, struct line *l) 
{
	struct channel_state *c = find_channel(s, l->args[2]);
	struct banlist_entry *be;
	
	if(!c) {
		log_network_state(s, "Can't add banlist entries to %s: channel not found", l->args[2]);
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
	else log_network_state(s, "Can't end banlist for %s: channel not found\n", l->args[2]);
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
	struct network_nick *nn = find_network_nick(s, line_get_nick(l));

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
	if(is_channelname(l->args[1], &s->info)) {
		struct channel_state *c = find_channel(s, l->args[1]);
		struct channel_nick *n;
		char p;
		int arg = 2;
		for(i = 0; l->args[2][i]; i++) {
			switch(l->args[2][i]) {
				case '+': t = ADD; break;
				case '-': t = REMOVE; break;
				case 'b': /* Ban */
						  {
							  struct banlist_entry *be = g_new0(struct banlist_entry, 1);
							  be->time_set = time(NULL);
							  be->hostmask = g_strdup(l->args[arg]);
							  be->by = g_strdup(line_get_nick(l));
							  c->banlist = g_list_append(c->banlist, be);
						  }
												
						  arg++;
						  break;
				case 'l':
					if(!l->args[++arg]) {
						log_network_state(s, "Mode l requires argument, but no argument found");
						break;
					}
					c->limit = atol(l->args[arg]);
					c->modes['l'] = t;
					break;
				case 'k':
					if(!l->args[++arg]) {
						log_network_state(s, "Mode k requires argument, but no argument found");
						break;
					}

					g_free(c->key);
					if(t) { c->key = g_strdup(l->args[arg]); }
					else c->key = NULL;

					c->modes['k'] = t;
					break;
				default:
					  p = get_prefix_by_mode(l->args[2][i], &s->info);
					  if(p == ' ') {
						  c->modes[(unsigned char)l->args[2][i]] = t;
					  } else {
							n = find_channel_nick(c, l->args[++arg]);
							if(!n) {
								log_network_state(s, "Can't set mode %c%c on nick %s on channel %s, because nick does not exist!", t == ADD?'+':'-', l->args[2][i], l->args[arg], l->args[1]);
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
	s->info.supported_user_modes = g_strdup(l->args[4]);
	s->info.supported_channel_modes = g_strdup(l->args[5]);
	s->info.server = g_strdup(l->args[2]);
}

static void handle_privmsg(struct network_state *s, struct line *l)
{
	struct network_nick *nn;
	if (irccmp(&s->info, l->args[1], s->me.nick) != 0) return;

	nn = find_add_network_nick(s, line_get_nick(l));
	nn->query = 1;
}

static void handle_nick(struct network_state *s, struct line *l)
{
	struct network_nick *nn;
	nn = find_add_network_nick(s, line_get_nick(l));
	network_nick_set_nick(nn, l->args[1]);
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

void state_handle_data(struct network_state *s, struct line *l)
{
	int i,j;

	if(!s || !l->args || !l->args[0])return;

	for(i = 0; irc_commands[i].command; i++) {
		if(!g_strcasecmp(irc_commands[i].command, l->args[0])) {
			for(j = 0; j <= irc_commands[i].min_args; j++) {
				if(!l->args[j])return;
			}
			irc_commands[i].handler(s,l);
			return;
		}
	}
}

struct network_state *new_network_state(const char *nick, const char *username, const char *hostname)
{
	struct network_state *state = g_new0(struct network_state, 1);
	state->info.features = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	state->me.query = 1;
	network_nick_set_data(&state->me, nick, username, hostname);

	state->nicks = g_list_append(state->nicks, &state->me);

	return state;
}

void free_network_nick(struct network_state *st, struct network_nick *nn)
{
	g_assert(&st->me != nn);
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
	while(state->channels)
	{
		free_channel((struct channel_state *)state->channels->data);
	}

	state->nicks = g_list_remove(state->nicks, &state->me);

	g_free(state->me.nick);
	g_free(state->me.username);
	g_free(state->me.hostname);
	g_free(state->me.hostmask);

	while(state->nicks) 
	{
		struct network_nick *nn = state->nicks->data;
		free_network_nick(state, nn);
	}

	g_free(state->info.supported_user_modes);
	state->info.supported_user_modes = NULL;

	g_free(state->info.supported_channel_modes);
	state->info.supported_channel_modes = NULL;

	g_hash_table_destroy(state->info.features);
	state->info.features = NULL;
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

static gboolean marshall_bytes (struct network_state *nst, enum marshall_mode m, struct data_blob *t, void **d, size_t length)
{
	if (m == MARSHALL_PULL) {
		if (t->offset + sizeof(size_t) > t->length)
			return FALSE;

		memcpy(&length, t->data+t->offset, sizeof(size_t));
		t->offset += sizeof(size_t);

		*d = g_malloc(length);

		memcpy(*d, t->data+t->offset, length);

		t->offset += length;
	} else {
		blob_increase_size(t, sizeof(size_t) + length);

		memcpy(t->data+t->offset, &length, sizeof(size_t));
		t->offset += sizeof(size_t);

		memcpy(t->data+t->offset, *d, length);

		t->offset += length;
	}

	return TRUE;
}

static gboolean marshall_string (struct network_state *nst, enum marshall_mode m, struct data_blob *t, char **d)
{
	if (*d == NULL)
		return TRUE;

	if (m == MARSHALL_PULL)
		return marshall_bytes(nst, m, t, (void **)d, -1);
	else
		return marshall_bytes(nst, m, t, (void **)d, strlen(*d)+1);
}

static gboolean marshall_network_nick (struct network_state *nst, enum marshall_mode m, struct data_blob *t, struct network_nick **n)
{
	gboolean ret = marshall_bytes(nst, m, t, (void **)n, sizeof(struct network_nick));
	ret &= marshall_string(nst, m, t, &(*n)->nick);
	ret &= marshall_string(nst, m, t, &(*n)->fullname);
	ret &= marshall_string(nst, m, t, &(*n)->username);
	ret &= marshall_string(nst, m, t, &(*n)->hostname);
	ret &= marshall_string(nst, m, t, &(*n)->hostmask);
	if (m == MARSHALL_PULL)
		(*n)->channel_nicks = NULL;
	return ret;
}

struct ht_traverse_data 
{
	marshall_fn_t key_fn;
	marshall_fn_t val_fn;
	struct data_blob *data;
	struct network_state *nst;
};

static void marshall_GHashTable_helper (gpointer key, gpointer value, gpointer user_data)
{
	struct ht_traverse_data *td = user_data;
	td->key_fn(td->nst, MARSHALL_PUSH, td->data, &key);
	td->val_fn(td->nst, MARSHALL_PUSH, td->data, &value);
}

static gboolean marshall_GHashTable (struct network_state *nst, enum marshall_mode m, struct data_blob *t, GHashTable **gh, marshall_fn_t key_fn, marshall_fn_t val_fn)
{
	if (m == MARSHALL_PULL) {
		uint32_t count;
		int i;

		if (t->offset + sizeof(size_t) > t->length)
			return FALSE;

		memcpy(&count, t->data+t->offset, sizeof(size_t));
		
		t->offset += sizeof(size_t);

		*gh = NULL;

		for (i = 0; i < count; i++) {
			void *k, *v;
			if (!key_fn(nst, m, t, &k))
				return FALSE;
			if (!val_fn(nst, m, t, &v))
				return FALSE;
			g_hash_table_insert(*gh, k, v);
		}
	} else {
		uint32_t count = g_hash_table_size(*gh);
		struct ht_traverse_data td;
		blob_increase_size(t, sizeof(size_t));

		memcpy(t->data+t->offset, &count, sizeof(size_t));

		t->offset += sizeof(size_t);

		td.key_fn = key_fn;
		td.val_fn = val_fn;
		td.data = t;
		td.nst = nst;

		g_hash_table_foreach(*gh, marshall_GHashTable_helper, &td);
	}

	return TRUE;
}

static gboolean marshall_GList (struct network_state *nst, enum marshall_mode m, struct data_blob *t, GList **gl, marshall_fn_t marshall_fn)
{
	if (m == MARSHALL_PULL) {
		uint32_t count;
		int i;

		if (t->offset + sizeof(size_t) > t->length)
			return FALSE;

		memcpy(&count, t->data+t->offset, sizeof(size_t));
		
		t->offset += sizeof(size_t);

		*gl = NULL;

		for (i = 0; i < count; i++) {
			void *v;
			if (!marshall_fn(nst, m, t, &v))
				return FALSE;
			*gl = g_list_append(*gl, v);
		}
	} else {
		uint32_t count = g_list_length(*gl);
		GList *l;
		blob_increase_size(t, sizeof(size_t));

		memcpy(t->data+t->offset, &count, sizeof(size_t));

		t->offset += sizeof(size_t);

		for (l = *gl; l; l = l->next) {
			if (!marshall_fn(nst, m, t, &l->data))
				return FALSE;
		}
	}

	return TRUE;
}

static gboolean marshall_banlist_entry (struct network_state *nst, enum marshall_mode m, struct data_blob *t, struct banlist_entry **d)
{
	gboolean ret = marshall_bytes(nst, m, t, (void **)d, sizeof(struct banlist_entry));
	ret &= marshall_string(nst, m, t, &(*d)->hostmask);
	ret &= marshall_string(nst, m, t, &(*d)->by);
	return ret;
}

static gboolean marshall_channel_state (struct network_state *nst, enum marshall_mode m, struct data_blob *t, struct channel_state **c)
{
	gboolean ret = marshall_bytes(nst, m, t, (void **)c, sizeof(struct channel_state));
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

		if (t->offset + sizeof(size_t) > t->length)
			return FALSE;

		memcpy(&count, t->data+t->offset, sizeof(size_t));
		
		t->offset += sizeof(size_t);

		for (i = 0; i < count; i++) {
			char *mode;
			struct channel_nick *cn;
			char *nick;
			
			if (!marshall_bytes(nst, m, t, (void **)&mode, 1))
				return FALSE;

			if (!marshall_string(nst, m, t, &nick))
				return FALSE;
			
			cn = find_add_channel_nick(*c, nick);
			g_assert(cn);
			cn->mode = mode[0];

			g_free(nick);
			g_free(mode);
		}
	} else {
		uint32_t count = g_list_length((*c)->nicks);
		GList *l;
		blob_increase_size(t, sizeof(size_t));

		memcpy(t->data+t->offset, &count, sizeof(size_t));

		t->offset += sizeof(size_t);

		for (l = (*c)->nicks; l; l = l->next) {
			struct channel_nick *cn = l->data;
			if (!marshall_bytes(nst, m, t, (void **)&cn->mode, 1))
				return FALSE;

			if (!marshall_string(nst, m, t, &cn->global_nick->nick))
				return FALSE;
		}
	}

	return ret;
}

static gboolean marshall_network_info (struct network_state *nst, enum marshall_mode m, struct data_blob *t, struct network_info **n)
{
	gboolean ret = TRUE;
	ret &= marshall_bytes(nst, m, t, (void **)n, sizeof(struct network_info));
	ret &= marshall_string(nst, m, t, &(*n)->name);
	ret &= marshall_string(nst, m, t, &(*n)->server);
	ret &= marshall_string(nst, m, t, &(*n)->supported_user_modes);
	ret &= marshall_string(nst, m, t, &(*n)->supported_channel_modes);
	marshall_GHashTable(nst, m, t, &(*n)->features, (marshall_fn_t)marshall_string, (marshall_fn_t)marshall_string);
	return ret;
}



static gboolean marshall_network_state (struct network_state *nst, enum marshall_mode m, struct data_blob *t, struct network_state **n)
{
	struct network_nick *me_p;
	struct network_info *info_p;
	gboolean ret = TRUE;
	ret &= marshall_bytes(NULL, m, t, (void **)n, sizeof(struct network_state));
	ret &= marshall_GList(*n, m, t, &(*n)->nicks, (marshall_fn_t)marshall_network_nick);
	ret &= marshall_GList(*n, m, t, &(*n)->channels, (marshall_fn_t)marshall_channel_state);
	me_p = &(*n)->me;
	info_p = &(*n)->info;
	ret &= marshall_network_nick(*n, m, t, &me_p);
	ret &= marshall_network_info(*n, m, t, &info_p);

	if (m == MARSHALL_PULL) {
		memcpy(&(*n)->me, me_p, sizeof(*me_p));
		memcpy(&(*n)->info, info_p, sizeof(*info_p));
	}
	return ret;
}


struct network_state *network_state_decode(char *blob, size_t len)
{
	struct network_state *ret;
	struct data_blob db;
	db.data = blob;
	db.offset = 0;
	db.length = len;

	if (!marshall_network_state(NULL, MARSHALL_PULL, &db, &ret)) 
		return NULL;

	return ret;
}

char *network_state_encode(struct network_state *st, size_t *len)
{
	struct data_blob db;
	db.data = NULL;
	db.offset = 0;
	db.length = 0;
	*len = 0;
	
	if (!marshall_network_state(st, MARSHALL_PUSH, &db, &st))
		return NULL;
	
	*len = db.offset;
	return db.data;
}
