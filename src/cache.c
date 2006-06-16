/*
	ctrlproxy: A modular IRC proxy
	(c) 2005 Jelmer Vernooij <jelmer@nl.linux.org>

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "internals.h"
#include "irc.h"

static void client_send_nameslist(struct client *client, struct channel_state *channel)
{
	GList *gl;
	char cmode[2];

	g_assert(client);
	g_assert(channel);
	
	cmode[0] = channel->mode;
	cmode[1] = 0;

	for (gl = channel->nicks; gl; gl = gl->next)
	{
		struct channel_nick *cn = gl->data;
		char *tmp;

		g_assert(cn->global_nick);

		if (cn->mode) {
			tmp = g_strdup_printf("%c%s", cn->mode, cn->global_nick->nick);
		} else {
			tmp = g_strdup(cn->global_nick->nick);
		}

		g_assert(channel->name);
		client_send_response(client, RPL_NAMREPLY, cmode, channel->name, tmp, NULL);
		g_free(tmp);
	}

	client_send_response(client, RPL_ENDOFNAMES, channel->name, "End of /NAMES list", NULL);
}


static void client_send_banlist(struct client *client, struct channel_state *channel)
{
	GList *gl;

	g_assert(channel);
	g_assert(client);

	for (gl = channel->banlist; gl; gl = gl->next)
	{
		struct banlist_entry *be = gl->data;
		g_assert(be);
		client_send_response(client, RPL_BANLIST, channel->name, be->hostmask, NULL);
	}

	client_send_response(client, RPL_ENDOFBANLIST, channel->name, "End of channel ban list", NULL);
}

static gboolean client_try_cache_join(struct client *c, struct line *l)
{
	struct channel_state *ch;

	g_assert(c);
	g_assert(l);

	if (l->argc < 2) return FALSE;

	/* Only optimize easy queries :-) */
	if (strchr(l->args[1], ',')) return FALSE;
		
	ch = find_channel(c->network->state, l->args[1]);
	return (ch != NULL);
}

static gboolean client_try_cache_mode(struct client *c, struct line *l)
{
	int i;
	char m;
	struct channel_state *ch;

	g_assert(l);
	g_assert(c);

	if (l->argc < 2) return FALSE;
	
	/* Only optimize easy queries... */
	if (strchr(l->args[1], ',')) return FALSE;
		
	/* Only queries in the form of MODE #channel mode */
	if (l->argc != 3) return FALSE; 

	g_assert(c->network);
	g_assert(c->network->state);

	ch = find_channel(c->network->state, l->args[1]);
	if (!ch) return FALSE;

	for (i = 0; (m = l->args[2][i]); i++) {
		switch (m) {
		case 'b': client_send_banlist(c, ch); break;
		default: return FALSE;
		}
	}

	return TRUE;
}

static gboolean client_try_cache_topic(struct client *c, struct line *l)
{
	struct channel_state *ch;

	if (l->argc < 2) return FALSE;
	
	/* Only optimize easy queries... */
	if (strchr(l->args[1], ',')) return FALSE;

	/* No set requests */
	if (l->args[2]) return FALSE;
	
	ch = find_channel(c->network->state, l->args[1]);
	if (!ch) return FALSE;

	if(ch->topic) {
		client_send_response(c, RPL_TOPIC, ch->name, ch->topic, NULL);
	} else {
		client_send_response(c, RPL_NOTOPIC, ch->name, "No topic set", NULL);
	}

	return TRUE;
}

static gboolean client_try_cache_names(struct client *c, struct line *l)
{
	struct channel_state *ch;

	g_assert(l);
	g_assert(c);

	if (l->argc < 2) return FALSE;
	
	/* Only optimize easy queries... */
	if (strchr(l->args[1], ',')) return FALSE;

	if (l->argc != 2) return FALSE;

	g_assert(c->network);
	g_assert(c->network->state);

	ch = find_channel(c->network->state, l->args[1]);
	if (!ch) return FALSE;

	client_send_nameslist(c, ch);

	return TRUE;
}

struct cache_command {
	const char *name;
	/* Should return FALSE if command couldn't be cached */
	gboolean (*try_cache) (struct client *c, struct line *l);
} cache_commands[] = {
	{ "JOIN", client_try_cache_join },
	{ "MODE", client_try_cache_mode },
	{ "NAMES", client_try_cache_names },
	{ "TOPIC", client_try_cache_topic },
	{ NULL, NULL }
};

/* Try to answer a client query from cache */
gboolean client_try_cache(struct client *c, struct line *l)
{
	g_assert(l);
	int i;

	if (l->argc == 0) 
		return TRUE;

	for (i = 0; cache_commands[i].name; i++) {
		if (!g_strcasecmp(l->args[0], cache_commands[i].name))
			return cache_commands[i].try_cache(c, l);
	}
	
	return FALSE;
}