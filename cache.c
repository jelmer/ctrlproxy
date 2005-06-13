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

static void client_send_banlist(struct client *client, struct channel_state *channel)
{
	GList *gl;
	for (gl = channel->banlist; gl; gl = gl->next)
	{
		struct banlist_entry *be = gl->data;
		client_send_response(client, RPL_BANLIST, channel->name, be->hostmask, NULL);
	}

	client_send_response(client, RPL_ENDOFBANLIST, "End of channel ban list", NULL);
}

static gboolean client_try_cache_join(struct line *l)
{
	struct channel_state *c;

	/* Only optimize easy queries :-) */
	if (strchr(l->args[1], ',')) return FALSE;
		
	c = find_channel(l->client->network, l->args[1]);
	return (c && c->joined);
}

static gboolean client_try_cache_mode(struct line *l)
{
	int i;
	char m;
	struct channel_state *c;

	/* Only optimize easy queries... */
	if (strchr(l->args[1], ',')) return FALSE;
		
	/* Only queries in the form of MODE #channel mode */
	if (l->argc != 3) return FALSE; 

	c = find_channel(l->client->network, l->args[1]);
	if (!c) return FALSE;

	for (i = 0; (m = l->args[2][i]); i++) {
		switch (m) {
		case 'b': client_send_banlist(l->client, c); break;
		default: return FALSE;
		}
	}

	return TRUE;
}

static gboolean client_try_cache_who(struct line *l)
{
	struct channel_state *c;
	
	/* Only optimize easy queries... */
	if (strchr(l->args[1], ',')) return FALSE;

	if (l->argc != 2) return FALSE;

	c = find_channel(l->client->network, l->args[1]);
	if (!c) return FALSE;

	return FALSE; /* FIXME */
}

/* Try to answer a client query from cache */
gboolean client_try_cache(struct line *l)
{
	if (l->argc == 0) return TRUE;

	if (!g_strcasecmp(l->args[0], "JOIN")) {
		return client_try_cache_join(l);
	}
	
	if (!g_strcasecmp(l->args[0], "MODE")) {
		return client_try_cache_mode(l);
	}

	if (!g_strcasecmp(l->args[0], "WHO")) {
		return client_try_cache_who(l);
	}

	return FALSE;
}
