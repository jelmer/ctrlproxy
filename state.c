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

enum mode_type { REMOVE = 0, ADD = 1};

char *mode2string(char modes[255])
{
	static char ret[255];
	unsigned char i;
	int pos = 0;
	for(i = 0; i < 255; i++) {
		if(modes[i]) { ret[pos] = (char)i; pos++; }
	}
	return ret;
}

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

void network_nick_set_hostmask(struct network_nick *n, const char *hm)
{
	/* FIXME */
}

static void free_nick(struct channel_nick *n)
{
	n->global_nick->channels = g_list_remove(n->global_nick->channels, n->channel);
	n->global_nick->refcount--;
	if(n->global_nick->refcount == 0) {
		if(n->global_nick->hostmask != NULL)
			g_free(n->global_nick->hostmask);
		g_free(n->global_nick->nick);
		n->channel->network->state.nicks = g_list_remove(n->channel->network->state.nicks, n->global_nick);
		g_free(n->global_nick);
	}
	g_free(n);
}


static void free_invitelist(struct channel *c)
{
	GList *g = c->invitelist;
	while(g) {
		g_free(g->data);
		g = g_list_remove(g, g->data);
	}
	c->invitelist = NULL;
}

static void free_exceptlist(struct channel *c)
{
	GList *g = c->exceptlist;
	while(g) {
		g_free(g->data);
		g = g_list_remove(g, g->data);
	}
	c->exceptlist = NULL;
}

static void free_banlist(struct channel *c)
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

static void free_names(struct channel *c)
{
	GList *g = c->nicks;
	while(g) {
		free_nick((struct channel_nick *)g->data);
		g = g_list_next(g);
	}
	g_list_free(c->nicks);
	c->nicks = NULL;
}

static void free_channel(struct channel *c)
{
	free_names(c);
	g_free(c->name);
	g_free(c->topic);
	g_free(c->key);
	c->network->state.channels = g_list_remove(c->network->state.channels, c);
	g_free(c);
}

void free_channels(struct network *s)
{
	while(s->state.channels)
	{
		free_channel((struct channel *)s->state.channels->data);
	}
}

struct channel *find_channel(struct network *st, const char *name)
{
	GList *cl = st->state.channels;
	while(cl) {
		struct channel *c = (struct channel *)cl->data;
		if(!irccmp(st, c->name, name)) return c;
		cl = g_list_next(cl);
	}
	return NULL;
}

struct channel *find_add_channel(struct network *st, char *name) {
	struct channel *c = find_channel(st, name);
	if(c)return c;
	c = g_new(struct channel,1);
	memset(c, 0, sizeof(struct channel));
	c->network = st;
	st->state.channels = g_list_append(st->state.channels, c);
	c->name = g_strdup(name);

	return c;
}

struct channel_nick *find_nick(struct channel *c, const char *name) {
	GList *l = c->nicks;
	struct channel_nick *n;
	const char *realname = name;
	if(is_prefix(realname[0], c->network))realname++;

	while(l) {
		n = (struct channel_nick *)l->data;
		if(!irccmp(c->network, n->global_nick->nick, realname))return n;
		l = g_list_next(l);
	}

	return NULL;
}

static struct network_nick *find_add_network_nick(struct network *n, char *name)
{
	GList *gl = n->state.nicks;
	struct network_nick *nd;

	/* search for a existing global object*/
	while(gl) {
		struct network_nick *ndd = (struct network_nick*)gl->data;
		if(!irccmp(n, ndd->nick, name)) {
			ndd->refcount++;
			return ndd;
		}
		gl = gl->next;
	}

	/* create one, if it doesn't exist */
	nd = g_new0(struct network_nick,1);
	nd->refcount = 1;
	nd->nick = g_strdup(name);
	nd->hostmask = NULL;
	nd->channels = NULL;
	
	n->state.nicks = g_list_append(n->state.nicks, nd);
	return nd;
}

struct channel_nick *find_add_nick(struct channel *c, char *name) {
	struct channel_nick *n = find_nick(c, name);
	char *realname = name;
	if(n) return n;
	if(strlen(name) == 0)return NULL;

	n = g_new0(struct channel_nick,1);
	if(is_prefix(realname[0], c->network)) {
		n->mode = realname[0];
		realname++;
	}
	n->channel = c;
	n->global_nick = find_add_network_nick(c->network, realname);
	c->nicks = g_list_append(c->nicks, n);
	n->global_nick->channels = g_list_append(n->global_nick->channels, c);
	return n;
}

static void handle_join(struct network *s, struct line *l)
{
	struct channel *c;
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
			ni = find_add_nick(c, line_get_nick(l));
			if(ni->global_nick->hostmask)
				g_free(ni->global_nick->hostmask);
			ni->global_nick->hostmask = g_strdup(l->origin);

			/* The user is joining a channel */

			if(!irccmp(s, line_get_nick(l), s->state.me->nick)) {
				c->joined = TRUE;
				log_network(NULL, s, "Joining channel %s", c->name);
			}
		}
		p = n+1;
	}
	g_free(name);
}


static void handle_part(struct network *s, struct line *l)
{
	struct channel *c;
	struct channel_nick *n;
	int cont = 1;
	char *name = g_strdup(l->args[1]), *p, *m;

	p = name;
	if(!line_get_nick(l))return;

	while(cont) {
		m = strchr(p, ',');
		if(!m) cont = 0;
		else *m = '\0';

		c = find_channel(s, p);

		/* The user is joining a channel */
		if(!irccmp(s, line_get_nick(l), s->state.me->nick) && c) {
			log_network(NULL, s, "Leaving %s", p);
			c->joined = FALSE;
			free_channel(c);
			c = NULL;
			p = m + 1;
			continue;
		}

		if(c){
			n = find_nick(c, line_get_nick(l));
			if(n) {
				c->nicks = g_list_remove(c->nicks, n);
				free_nick(n);
			} else log_network(NULL, l->network, "Can't remove nick %s from channel %s: nick not on channel", line_get_nick(l), p);

			continue;
		}

		log_network(NULL, l->network, "Can't part or let other nick part %s(unknown channel)", p);
		p = m + 1;
	}
	g_free(name);
}

static void handle_kick(struct network *s, struct line *l) {
	struct channel *c;
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
			log_network(NULL, s, "KICK command has unequal number of channels and nicks");
		}

		if(nextnick && nextchan) cont = 1;
		else cont = 0;

		c = find_channel(s, curchan);

		if(!c){
			log_network(NULL, s, "Can't kick nick %s from %s", curnick, curchan);
			curchan = nextchan; curnick = nextnick;
			continue;
		}

		n = find_nick(c, curnick);
		if(!n) {
			log_network(NULL, s, "Can't kick nick %s from channel %s: nick not on channel", curnick, curchan);
			curchan = nextchan; curnick = nextnick;
			continue;
		}

		if(!g_strcasecmp(n->global_nick->nick, s->state.me->nick))
			c->joined = FALSE;

		c->nicks = g_list_remove(c->nicks, n);
		free_nick(n);
		curchan = nextchan; curnick = nextnick;
	}
}

static void handle_topic(struct network *s, struct line *l) {
	struct channel *c = find_channel(s, l->args[1]);
	if(c->topic)g_free(c->topic);
	c->topic = g_strdup(l->args[2]);
}

static void handle_332(struct network *s, struct line *l) {
	struct channel *c = find_channel(s, l->args[2]);

	if(!c) {
		log_network(NULL, s, "Can't set topic for unknown channel '%s'!", l->args[2]);
		return;
	}

	if(c->topic)g_free(c->topic);
	c->topic = g_strdup(l->args[3]);
}

static void handle_no_topic(struct network *s, struct line *l) {
	struct channel *c = find_channel(s, l->args[1]);

	if(!c) {
		log_network(NULL, s, "Can't unset topic for unknown channel '%s'!", l->args[2]);
		return;
	}

	if(c->topic)g_free(c->topic);
	c->topic = NULL;
}

static void handle_namreply(struct network *s, struct line *l) {
	char *names, *tmp, *t;
	struct channel *c = find_channel(s, l->args[3]);
	if(!c) {
		log_network(NULL, s, "Can't add names to %s: channel not found", l->args[3]);
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
		if(tmp[0])find_add_nick(c, tmp);
		tmp = t+1;
	}
	if(tmp[0])find_add_nick(c, tmp);
	g_free(names);
}

static void handle_end_names(struct network *s, struct line *l) {
	struct channel *c = find_channel(s, l->args[2]);
	if(c)c->namreply_started = FALSE;
	else log_network(NULL, s, "Can't end /NAMES command for %s: channel not found\n", l->args[2]);
}

static void handle_invitelist_entry(struct network *s, struct line *l) 
{
	struct channel *c = find_channel(s, l->args[2]);
	
	if(!c) {
		log_network(NULL, s, "Can't add invitelist entries to %s: channel not found", l->args[2]);
		return;
	}

	if (!c->invitelist_started) {
		free_invitelist(c);
		c->invitelist_started = TRUE;
	}

	c->invitelist = g_list_append(c->invitelist, g_strdup(l->args[3]));
}

static void handle_end_invitelist(struct network *s, struct line *l) 
{
	struct channel *c = find_channel(s, l->args[2]);
	if(c)c->invitelist_started = FALSE;
	else log_network(NULL, s, "Can't end invitelist for %s: channel not found\n", l->args[2]);
}

static void handle_exceptlist_entry(struct network *s, struct line *l) 
{
	struct channel *c = find_channel(s, l->args[2]);
	
	if(!c) {
		log_network(NULL, s, "Can't add exceptlist entries to %s: channel not found", l->args[2]);
		return;
	}

	if (!c->exceptlist_started) {
		free_exceptlist(c);
		c->exceptlist_started = TRUE;
	}

	c->exceptlist = g_list_append(c->exceptlist, g_strdup(l->args[3]));
}

static void handle_end_exceptlist(struct network *s, struct line *l) 
{
	struct channel *c = find_channel(s, l->args[2]);
	if(c)c->exceptlist_started = FALSE;
	else log_network(NULL, s, "Can't end exceptlist for %s: channel not found\n", l->args[2]);
}



static void handle_banlist_entry(struct network *s, struct line *l) 
{
	struct channel *c = find_channel(s, l->args[2]);
	struct banlist_entry *be;
	
	if(!c) {
		log_network(NULL, s, "Can't add banlist entries to %s: channel not found", l->args[2]);
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

static void handle_end_banlist(struct network *s, struct line *l) 
{
	struct channel *c = find_channel(s, l->args[2]);
	if(c)c->banlist_started = FALSE;
	else log_network(NULL, s, "Can't end banlist for %s: channel not found\n", l->args[2]);
}

static void handle_whoreply(struct network *s, struct line *l) {
	struct channel_nick *n; struct channel *c;

	c = find_channel(s, l->args[2]);
	if(!c) 
		return;

	n = find_add_nick(c, l->args[6]);
	network_nick_set_data(n->global_nick, l->args[6], l->args[3], l->args[4]);
}

static void handle_end_who(struct network *s, struct line *l) {
}

static void handle_quit(struct network *s, struct line *l) {
	GList *g = s->state.channels;
	if(!line_get_nick(l))return;
	while(g) {
		struct channel *c = (struct channel *)g->data;
		struct channel_nick *n = find_nick(c, line_get_nick(l));
		if(n) {
			c->nicks = g_list_remove(c->nicks, n);
			free_nick(n);
		}
		g = g_list_next(g);
	}
}

static void handle_mode(struct network *s, struct line *l)
{
	/* Format:
	 * MODE %|<nick>|<channel> [<mode> [<mode parameters>]] */
	enum mode_type t = ADD;
	int i;

	/* We only care about channel modes and our own mode */

	/* Own nick is being changed */
	if(!strcmp(l->args[1], s->state.me->nick)) {
		for(i = 0; l->args[2][i]; i++) {
			switch(l->args[2][i]) {
				case '+': t = ADD;break;
				case '-': t = REMOVE; break;
				default:
					  s->state.me->modes[(unsigned char)l->args[2][i]] = t;
					  break;
			}
		}

	} else if(is_channelname(l->args[1], l->network)) {
		struct channel *c = find_channel(s, l->args[1]);
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
						log_network(NULL, s, "Mode l requires argument, but no argument found");
						break;
					}
					c->limit = atol(l->args[arg]);
					c->modes['l'] = t;
					break;
				case 'k':
					if(!l->args[++arg]) {
						log_network(NULL, s, "Mode k requires argument, but no argument found");
						break;
					}

					g_free(c->key);
					if(t) { c->key = g_strdup(l->args[arg]); }
					else c->key = NULL;

					c->modes['k'] = t;
					break;
				default:
					  p = get_prefix_by_mode(l->args[2][i], l->network);
					  if(p == ' ') {
						  c->modes[(unsigned char)l->args[2][i]] = t;
					  } else {
							n = find_nick(c, l->args[++arg]);
							if(!n) {
								log_network(NULL, s, "Can't set mode %c%c on nick %s on channel %s, because nick does not exist!", t == ADD?'+':'-', l->args[2][i], l->args[arg], l->args[1]);
								break;
							}
							n->mode = (t == ADD?p:' ');
					  }
					  break;
			}
		}
	}
}

static void handle_004(struct network *s, struct line *l)
{
	s->state.info.supported_user_modes = g_strdup(l->args[4]);
	s->state.info.supported_channel_modes = g_strdup(l->args[5]);

	/* Make sure the current server is listed as a possible 
	 * one for this network */
	if (l->network->connection.type == NETWORK_TCP) {
		GList *gl;
		for (gl = l->network->connection.data.tcp.servers; gl; gl = gl->next) {
			struct tcp_server *tcp = gl->data;

			if (!g_strcasecmp(tcp->host, l->args[1])) 
				break;
		}

		/* Not found, add new one */
		if (!gl) {
			struct tcp_server *tcp = g_new0(struct tcp_server, 1);
			int error;
			struct addrinfo hints;

			tcp->host = g_strdup(l->args[2]);
			tcp->name = g_strdup(l->args[2]);
			tcp->port = g_strdup(l->network->connection.data.tcp.current_server->port);
			tcp->ssl = l->network->connection.data.tcp.current_server->ssl;
			tcp->password = g_strdup(l->network->connection.data.tcp.current_server->password);

			memset(&hints, 0, sizeof(hints));
			hints.ai_family = PF_UNSPEC;
			hints.ai_socktype = SOCK_STREAM;

			error = getaddrinfo(tcp->host, tcp->port, &hints, &tcp->addrinfo);
			if (error) {
				log_network(NULL, l->network, "Unable to lookup %s: %s", tcp->host, gai_strerror(error));
			} else {
				l->network->connection.data.tcp.servers = g_list_append(l->network->connection.data.tcp.servers, tcp);
			}
		}
	}
}


static void handle_nick(struct network *s, struct line *l)
{
	GList *g = s->state.channels;

	/* Server confirms messages client sends, so let's only handle those */
	if(!l->args[1] || !line_get_nick(l)) return;
	while(g) {
		struct channel *c = (struct channel *)g->data;
		struct channel_nick *n = find_nick(c, line_get_nick(l));
		if(n) {
			g_free(n->global_nick->nick);
			n->global_nick->nick = g_strdup(l->args[1]);
		}
		g = g_list_next(g);
	}
}

static void handle_465(struct network *s, struct line *l)
{
	log_network(NULL, l->network, "Banned from server: %s", l->args[1]);
}

static void handle_451(struct network *s, struct line *l)
{
	log_network(NULL, l->network, "Not registered error, this is probably a bug...");
}

static void handle_462(struct network *s, struct line *l)
{
	log_network(NULL, l->network, "Double registration error, this is probably a bug...");
}

static void handle_463(struct network *s, struct line *l)
{
	log_network(NULL, l->network, "Host not privileged to connect");
}

static void handle_464(struct network *s, struct line *l)
{
	log_network(NULL, l->network, "Password mismatch");
}

static void handle_302(struct network *s, struct line *l)
{
	/* We got a USERHOST response, split it into nick and user@host, and check the nick */
	gchar** tmp302 = g_strsplit(g_strstrip(l->args[2]), "=+", 2);
	if (g_strv_length(tmp302) > 1) {
		if (!irccmp(l->network, l->network->state.me->nick, tmp302[0])) {
			g_free(l->network->state.me->hostmask);
			/* Set the hostmask if it is our nick */
			l->network->state.me->hostmask = g_strdup_printf("%s!%s", tmp302[0], tmp302[1]);
		}
	}
	g_strfreev(tmp302);
}

extern void handle_005(struct network *s, struct line *l);

static struct irc_command {
	char *command;
	int min_args;
	void (*handler) (struct network *s, struct line *l);
} irc_commands[] = {
	{ "JOIN", 1, handle_join },
	{ "PART", 1, handle_part },
	{ "KICK", 2, handle_kick },
	{ "QUIT", 0, handle_quit },
	{ "TOPIC", 2, handle_topic },
	{ "NICK", 1, handle_nick },
	{ "MODE", 2, handle_mode },
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
	{ "451", 1, handle_451 },
	{ "462", 1, handle_462 },
	{ "463", 1, handle_463 },
	{ "464", 1, handle_464 },
	{ "465", 1, handle_465 },
	{ NULL }
};

void state_handle_data(struct network *s, struct line *l)
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
	return;
}


GSList *gen_replication_channel(struct channel *c, const char *hostmask, const char *nick)
{
	GSList *ret = NULL;
	struct channel_nick *n;
	GList *nl;
	ret = g_slist_append(ret, irc_parse_linef(":%s JOIN %s\r\n", nick, c->name));

	if(c->topic) {
		ret = g_slist_append(ret, irc_parse_linef(":%s 332 %s %s :%s\r\n", hostmask, nick, c->name, c->topic));
	} else {
		ret = g_slist_append(ret, irc_parse_linef(":%s 331 %s %s :No topic set\r\n", hostmask, nick, c->name));
	}

	nl = c->nicks;
	while(nl) {
		n = (struct channel_nick *)nl->data;
		if(n->mode && n->mode != ' ') { ret = g_slist_append(ret, irc_parse_linef(":%s 353 %s %c %s :%c%s\r\n", hostmask, nick, c->mode, c->name, n->mode, n->global_nick->nick)); }
		else { ret = g_slist_append(ret, irc_parse_linef(":%s 353 %s %c %s :%s\r\n", hostmask, nick, c->mode, c->name, n->global_nick->nick)); }
		nl = g_list_next(nl);
	}
	ret = g_slist_append(ret, irc_parse_linef(":%s 366 %s %s :End of /names list\r\n", hostmask, nick, c->name));
	c->introduced = 3;
	return ret;
}

GSList *gen_replication_network(struct network *s)
{
	GList *cl;
	struct channel *c;
	GSList *ret = NULL;
	cl = s->state.channels;

	while(cl) {
		c = (struct channel *)cl->data;
		if(!is_channelname(c->name, s)) {
			cl = g_list_next(cl);
			continue;
		}

		ret = g_slist_concat(ret, gen_replication_channel(c, s->name, s->state.me->nick));

		cl = g_list_next(cl);
	}

	if(strlen(mode2string(s->state.me->modes)))
		ret = g_slist_append(ret, irc_parse_linef(":%s MODE %s +%s\r\n", s->name, s->state.me->nick, mode2string(s->state.me->modes)));

	return ret;
}

struct linestack_context *linestack_new_by_network(struct network *n)
{
	return linestack_new(NULL, NULL);
}

void init_state(struct network_state *state, const char *nick, const char *username, const char *hostname)
{
	state->info.features = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	state->me = g_new0(struct network_nick, 1);
	state->me->refcount = 1;
	network_nick_set_data(state->me, nick, username, hostname);

	state->nicks = g_list_append(state->nicks, state->me);
}

void free_state(struct network_state *state)
{
	g_free(state->me->hostmask);
	state->me->hostmask = NULL;

	g_free(state->info.supported_user_modes);
	state->info.supported_user_modes = NULL;

	g_free(state->info.supported_channel_modes);
	state->info.supported_channel_modes = NULL;

	g_hash_table_destroy(state->info.features);
	state->info.features = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

}
