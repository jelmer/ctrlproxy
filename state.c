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

struct started_join {
	char *channel;
	void *network;
};

static GList *started_join_list = NULL;

int is_channelname(char *name, struct network *n)
{
	const char *chantypes = get_network_feature(n, "CHANTYPES");
	if(strchr(chantypes, name[0])) return 1;
	return 0;
}

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

static void free_nick(struct channel_nick *n)
{
	n->global->channels = g_list_remove(n->global->channels, n->channel);
	n->global->refcount--;
	if(n->global->refcount == 0) {
		if(n->global->hostmask != NULL)
			free(n->global->hostmask);
		free(n->global->name);
		n->channel->network->nicks = g_list_remove(n->channel->network->nicks, n->global);
		free(n->global);
	}
	free(n);
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
	if(c->topic)free(c->topic);
	c->topic = NULL;
}

struct channel *find_channel(struct network *st, char *name)
{
	GList *cl = st->channels;
	while(cl) {
		struct channel *c = (struct channel *)cl->data;
		char *channel_name = xmlGetProp(c->xmlConf, "name");
		if(!strcasecmp(channel_name, name)) { xmlFree(channel_name); return c; }
		xmlFree(channel_name);
		cl = g_list_next(cl);
	}
	return NULL;
}

struct channel *find_add_channel(struct network *st, char *name) {
	struct channel *c = find_channel(st, name);
	if(c)return c;
	c = malloc(sizeof(struct channel));
	memset(c, 0, sizeof(struct channel));
	c->network = st;
	st->channels = g_list_append(st->channels, c);

	/* check if there is a XML node for this channel yet */
	c->xmlConf = xmlFindChildByName(st->xmlConf, name);

	if(!c->xmlConf) {
		c->xmlConf = xmlNewNode(NULL, "channel");
		xmlSetProp(c->xmlConf, "name", name);
		xmlAddChild(st->xmlConf, c->xmlConf);
	}

	return c;
}

struct channel_nick *find_nick(struct channel *c, char *name) {
	GList *l = c->nicks;
	struct channel_nick *n;
	char *realname = name;
	if(realname[0] == '@' || realname[0] == '+')realname++;

	while(l) {
		n = (struct channel_nick *)l->data;
		if(!strcasecmp(n->global->name, realname))return n;
		l = g_list_next(l);
	}

	return NULL;
}

static struct network_nick *find_add_network_nick(struct network *n, char *name)
{
	GList *gl = n->nicks;
	struct network_nick *nd;

	/* search for a existing global object*/
	while(gl) {
		struct network_nick *ndd = (struct network_nick*)gl->data;
		if(!strcasecmp(ndd->name, name)) {
			ndd->refcount++;
			return ndd;
		}
		gl = gl->next;
	}

	/* create one, if it doesn't exist */
	nd = malloc(sizeof(struct network_nick));
	memset(nd, 0, sizeof(struct network_nick));
	nd->refcount = 1;
	nd->name = strdup(name);
	nd->hostmask = NULL;
	nd->channels = NULL;
	
	n->nicks = g_list_append(n->nicks, nd);
	return nd;
}

struct channel_nick *find_add_nick(struct channel *c, char *name) {
	struct channel_nick *n = find_nick(c, name);
	char *realname = name;
	if(n) return n;
	if(strlen(name) == 0)return NULL;

	n = malloc(sizeof(struct channel_nick));
	memset(n, 0, sizeof(struct channel_nick));
	if(realname[0] == '@' || realname[0] == '+') {
		n->mode = realname[0];
		realname++;
	}
	n->channel = c;
	n->global = find_add_network_nick(c->network, realname);
	c->nicks = g_list_append(c->nicks, n);
	n->global->channels = g_list_append(n->global->channels, c);
	return n;
}


static void handle_join(struct network *s, struct line *l)
{
	struct channel *c;
	struct channel_nick *ni;
	struct started_join *sj;
	int cont = 1;
	char *own_nick, *tmp;
	char *name = strdup(l->args[1]), *p, *n;

	p = name;

	while(cont) {
		n = strchr(p, ',');

		if(!n) cont = 0;
		else *n = '\0';

		/* Someone is joining a channel the user is on */
		if(l->direction == FROM_SERVER && line_get_nick(l)) {
			c = find_add_channel(s, p);
			ni = find_add_nick(c, line_get_nick(l));
			ni->global->hostmask = strdup((char *)l->origin);

			/* The user is joining a channel */
			own_nick = xmlGetProp(s->xmlConf, "nick");

			if(!strcasecmp(line_get_nick(l), own_nick)) {
				xmlSetProp(c->xmlConf, "autojoin", "1");
				g_message("Joining channel %s", p);

				/* send WHO command for updating hostmasks */
				sj = malloc(sizeof(struct started_join));
				sj->channel = strdup(p);
				sj->network = s;
				started_join_list = g_list_append(started_join_list, sj);
				asprintf(&tmp, "WHO %s\n", p);
				transport_write(s->outgoing, tmp);
				free(tmp);
			}

			xmlFree(own_nick);
		}
		p = n+1;
	}
	free(name);
}


static void handle_part(struct network *s, struct line *l)
{
	struct channel *c;
	struct channel_nick *n;
	int cont = 1;
	char *own_nick;
	char *name = strdup(l->args[1]), *p, *m;

	p = name;
	if(!line_get_nick(l))return;

	while(cont) {
		m = strchr(p, ',');
		if(!m) cont = 0;
		else *m = '\0';

		c = find_channel(s, p);

		/* The user is joining a channel */
		own_nick = xmlGetProp(s->xmlConf, "nick");

		if(!strcasecmp(line_get_nick(l), own_nick) && c) {
			s->channels = g_list_remove(s->channels, c);
			g_message("Leaving %s", p);
			xmlSetProp(c->xmlConf, "autojoin", "0");
			free_channel(c);
			free(c);
			c = NULL;
			xmlFree(own_nick);
			p = m + 1;
			continue;
		}

		xmlFree(own_nick);

		if(c){
			n = find_nick(c, line_get_nick(l));
			if(n) {
				c->nicks = g_list_remove(c->nicks, n);
				free_nick(n);
			} else g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "Can't remove nick %s from channel %s: nick not on channel\n", line_get_nick(l), p);

			continue;
		}

		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "Can't part or let other nick part %s(unknown channel)\n", p);
		p = m + 1;
	}
	free(name);
}

static void handle_kick(struct network *s, struct line *l) {
	struct channel *c;
	struct channel_nick *n;
	char *nicks = strdup(l->args[2]);
	char *channels = strdup(l->args[1]);
	char *curnick, *curchan, *nextchan, *nextnick;
	char cont = 1;

	curchan = channels; curnick = nicks;
	while(cont) {
		nextnick = strchr(curnick, ',');
		if(nextnick){ *nextnick = '\0'; nextnick++; }

		nextchan = strchr(curchan, ',');
		if(nextchan){ *nextchan = '\0'; nextchan++; }

		if((!nextnick && nextchan) || (nextnick && !nextchan)) {
			g_warning("KICK command has unequal number of channels and nicks\n");
		}

		if(nextnick && nextchan) cont = 1;
		else cont = 0;

		c = find_channel(s, curchan);
		if(!c){
			g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "Can't kick nick %s from %s\n", curnick, curchan);
			curchan = nextchan; curnick = nextnick;
			continue;
		}

		n = find_nick(c, curnick);
		if(!n) {
			g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "Can't kick nick %s from channel %s: nick not on channel\n", curnick, curchan);
			curchan = nextchan; curnick = nextnick;
			continue;
		}
		
		c->nicks = g_list_remove(c->nicks, n);
		free_nick(n);
		curchan = nextchan; curnick = nextnick;
	}
}

static void handle_topic(struct network *s, struct line *l) {
	struct channel *c = find_channel(s, l->args[1]);
	if(c->topic)free(c->topic);
	c->topic = strdup(l->args[2]);
}

static void handle_332(struct network *s, struct line *l) {
	struct channel *c = find_channel(s, l->args[2]);

	if(!c) {
		g_warning("Can't set topic for unknown channel '%s'!", l->args[2]);
		return;
	}

	if(c->topic)free(c->topic);
	c->topic = strdup(l->args[3]);
}

static void handle_no_topic(struct network *s, struct line *l) {
	struct channel *c = find_channel(s, l->args[1]);

	if(!c) {
		g_warning("Can't unset topic for unknown channel '%s'!", l->args[2]);
		return;
	}

	if(c->topic)free(c->topic);
	c->topic = NULL;
}

static void handle_namreply(struct network *s, struct line *l) {
	char *names, *tmp, *t;
	struct channel *c = find_channel(s, l->args[3]);
	if(!c) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "Can't add names to %s: channel not found\n", l->args[3]);
		return;
	}
	c->mode = l->args[2][0];
	if(c->namreply_started == 0) {
		free_names(c);
		c->namreply_started = 1;
	}
	tmp = names = strdup(l->args[4]);
	while((t = strchr(tmp, ' '))) {
		*t = '\0';
		find_add_nick(c, tmp);
		tmp = t+1;
	}
	find_add_nick(c, tmp);
	free(names);
}

static void handle_end_names(struct network *s, struct line *l) {
	struct channel *c = find_channel(s, l->args[2]);
	if(c)c->namreply_started = 0;
	else g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "Can't end /NAMES command for %s: channel not found\n", l->args[2]);
}

static void handle_whoreply(struct network *s, struct line *l) {
	char *hostmask;
	struct channel_nick *n; struct channel *c;
	GList *gl = started_join_list;
	/* don't send the who replay when caused by a join */
	while(gl) {
		struct started_join *sj = (struct started_join *)gl->data;
		if((s == sj->network) && (!strcmp(sj->channel, l->args[2]))) {
			l->options = l->options | LINE_DONT_SEND;
			break;
		}
		gl = gl->next;
	}
	c = find_channel(s, l->args[2]);
	if(!c) {
		return;
	}
	n = find_add_nick(c, strdup(l->args[6]));
	if(n->global->hostmask == NULL) {
		asprintf(&hostmask, "%s!%s@%s", l->args[6], l->args[3], l->args[4]);
		n->global->hostmask = hostmask;
	}
}

static void handle_end_who(struct network *s, struct line *l) {
	GList *gl = started_join_list;
	/* remove entry added by join and don't send line */
	while(gl) {
		struct started_join *sj = (struct started_join *)gl->data;
		if((s == sj->network) && (!strcmp(sj->channel, l->args[2]))) {
   			l->options = l->options | LINE_DONT_SEND;
			started_join_list = g_list_remove(started_join_list, sj);
			free(sj->channel);
			free(sj);
			break;
		}
		gl = gl->next;
	}
}


static void handle_quit(struct network *s, struct line *l) {
	GList *g = s->channels;
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
	char *nick;
	enum mode_type t = ADD;
	int i;

	if(l->direction == TO_SERVER)return;

	/* We only care about channel modes and our own mode */
	nick = xmlGetProp(s->xmlConf, "nick");
	g_assert(nick);
	/* Own nick is being changed */
	if(!strcmp(l->args[1], nick)) {
		for(i = 0; l->args[2][i]; i++) {
			switch(l->args[2][i]) {
				case '+': t = ADD;break;
				case '-': t = REMOVE; break;
				default:
					  s->mymodes[(unsigned char)l->args[2][i]] = t;
					  break;
			}
		}

	} else if(is_channelname(l->args[1], l->network)) {
		struct channel *c = find_channel(s, l->args[1]);
		struct channel_nick *n;
		int arg = 2;
		for(i = 0; l->args[2][i]; i++) {
			switch(l->args[2][i]) {
				case '+': t = ADD; break;
				case '-': t = REMOVE; break;
				case 'b': /* Don't do anything (like store, etc) with ban
							 for now */
						  arg++;
						  break;
				case 'o':
					n = find_nick(c, l->args[++arg]);
					if(!n) {
						g_error("Can't set mode %c%c on nick %s on channel %s, because nick does not exist!", t == ADD?'+':'-', l->args[2][i], l->args[arg], l->args[1]);
						break;
					}
					n->mode = (t == ADD?'@':' ');
					break;
				case 'v':
					n = find_nick(c, l->args[++arg]);
					if(!n) {
						g_error("Can't set mode %c%c on nick %s on channel %s, because nick does not exist!", t == ADD?'+':'-', l->args[2][i], l->args[arg], l->args[1]);
						break;
					}
					n->mode = (t == ADD?'+':' ');
					break;
				case 'l':
					if(!l->args[++arg]) {
						g_error("Mode l requires argument, but no argument found");
						break;
					}
					c->limit = atol(l->args[arg]);
					c->modes['l'] = t;
					break;
				case 'k':
					if(!l->args[++arg]) {
						g_error("Mode k requires argument, but no argument found");
						break;
					}

					if(t) xmlSetProp(c->xmlConf, "key", l->args[arg]);
					else xmlUnsetProp(c->xmlConf, "key");
					c->modes['k'] = t;
					break;
				default:
					  c->modes[(unsigned char)l->args[2][i]] = t;
					  break;
			}
		}
	}

	xmlFree(nick);
}

static void handle_004(struct network *s, struct line *l)
{
	if(l->direction == TO_SERVER)return;

	s->supported_modes[0] = strdup(l->args[4]);
	s->supported_modes[1] = strdup(l->args[5]);
}

static void handle_005(struct network *s, struct line *l)
{
	int i, j = 0;
	if(l->direction == TO_SERVER)return;

	s->features = malloc(sizeof(char *) * l->argc);

	for(i = 3; i < l->argc-1; i++) {
		s->features[j] = strdup(l->args[i]);
		j++;
	}

	s->features[j] = NULL;
}

static void handle_nick(struct network *s, struct line *l)
{
	char *own_nick;
	GList *g = s->channels;

	/* Server confirms messages client sends, so let's only handle those */
	if(l->direction != FROM_SERVER || !l->args[1] || !line_get_nick(l)) return;
	while(g) {
		struct channel *c = (struct channel *)g->data;
		struct channel_nick *n = find_nick(c, line_get_nick(l));
		if(n) {
			free(n->global->name);
			n->global->name = strdup(l->args[1]);
		}
		g = g_list_next(g);
	}

	own_nick = xmlGetProp(s->xmlConf, "nick");

	if(!strcasecmp(line_get_nick(l), own_nick)) {
		xmlSetProp(s->xmlConf, "nick", l->args[1]);
	}

	xmlFree(own_nick);
}

void state_reconnect(struct network *s)
{
	GList *l = s->channels;

	/* Remove list of channels */
	while(l) {
		struct channel *ch = (struct channel *)l->data;
		free_channel(ch);
		free(ch);
		l = g_list_next(l);
	}

	g_list_free(s->channels);

	s->channels = NULL;
}

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
	{ "004", 5, handle_004 },
	{ "005", 3, handle_005 },
	{ "332",  3, handle_332 },
	{ "331", 1, handle_no_topic },
	{ "353", 4, handle_namreply },
	{ "366", 2, handle_end_names },
	{ "352", 7, handle_whoreply },
	{ "315", 1, handle_end_who },
	{ "NICK", 1, handle_nick },
	{ "MODE", 2, handle_mode },
	{ NULL }
};

void state_handle_data(struct network *s, struct line *l)
{
	int i,j;

	if(!s || !l->args || !l->args[0])return;

	for(i = 0; irc_commands[i].command; i++) {
		if(!strcasecmp(irc_commands[i].command, l->args[0])) {
			for(j = 0; j <= irc_commands[i].min_args; j++) {
				if(!l->args[j])return;
			}
			irc_commands[i].handler(s,l);
			return;
		}
	}
	return;
}


GSList *gen_replication_channel(struct channel *c, char *hostmask, char *nick)
{
	GSList *ret = NULL;
	char *channel_name = xmlGetProp(c->xmlConf, "name");
	char *key = xmlGetProp(c->xmlConf, "key");
	struct channel_nick *n;
	GList *nl;
	ret = g_slist_append(ret, irc_parse_linef(":%s JOIN %s\r\n", nick, channel_name));

	xmlFree(key);

	if(c->topic) {
		ret = g_slist_append(ret, irc_parse_linef(":%s 332 %s %s :%s\r\n", hostmask, nick, channel_name, c->topic));
	} else {
		ret = g_slist_append(ret, irc_parse_linef(":%s 331 %s %s :No topic set\r\n", hostmask, nick, channel_name));
	}

	nl = c->nicks;
	while(nl) {
		n = (struct channel_nick *)nl->data;
		if(n->mode && n->mode != ' ') { ret = g_slist_append(ret, irc_parse_linef(":%s 353 %s %c %s :%c%s\r\n", hostmask, nick, c->mode, channel_name, n->mode, n->global->name)); }
		else { ret = g_slist_append(ret, irc_parse_linef(":%s 353 %s %c %s :%s\r\n", hostmask, nick, c->mode, channel_name, n->global->name)); }
		nl = g_list_next(nl);
	}
	ret = g_slist_append(ret, irc_parse_linef(":%s 366 %s %s :End of /names list\r\n", hostmask, nick, channel_name));
	c->introduced = 3;
	xmlFree(channel_name);
	return ret;
}

GSList *gen_replication_network(struct network *s)
{
	GList *cl;
	struct channel *c;
	GSList *ret = NULL;
	char *nick, *server_name, *channel_name;
	cl = s->channels,
	server_name = xmlGetProp(s->xmlConf, "name");
	nick = xmlGetProp(s->xmlConf, "nick");

	while(cl) {
		c = (struct channel *)cl->data;
		channel_name = xmlGetProp(c->xmlConf, "name");
		if(!is_channelname(channel_name, s)) {
			cl = g_list_next(cl);
			xmlFree(channel_name);
			continue;
		}
		xmlFree(channel_name);

		ret = g_slist_concat(ret, gen_replication_channel(c, server_name, nick));

		cl = g_list_next(cl);
	}

	if(strlen(mode2string(s->mymodes)))
		ret = g_slist_append(ret, irc_parse_linef(":%s MODE %s +%s\r\n", server_name, nick, mode2string(s->mymodes)));

	xmlFree(server_name);
	xmlFree(nick);

	return ret;
}

struct linestack_context *linestack_new_by_network(struct network *n)
{
	char *linestack, *linestack_location;
	struct linestack_context *ret;

	linestack = xmlGetProp(n->xmlConf, "linestack");
	linestack_location = xmlGetProp(n->xmlConf, "linestack_location");

	ret = linestack_new(linestack, linestack_location);

	xmlFree(linestack);
	xmlFree(linestack_location);

	return ret;
}

const char *get_network_feature(struct network *n, char *name)
{
	int i;
	for(i = 0; n->features[i]; i++) {
		if(strncmp(n->features[i], name, strlen(name) < strlen(n->features[i])?strlen(name):strlen(n->features[i]))) {
			char *eq = strchr(n->features[i], '=');
			if(eq) return eq+1;
			return "";		
		}
	}
	return NULL;
}
