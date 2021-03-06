/*
	ctrlproxy: A modular IRC proxy
	(c) 2005 Jelmer Vernooĳ <jelmer@jelmer.uk>

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "internals.h"
#include "irc.h"

static gboolean client_try_cache_mode(struct irc_client *c,
				      struct irc_network_state *net,
				      struct irc_line *l,
				      const struct cache_settings *settings)
{
	int i;
	char m;
	struct irc_channel_state *ch;

	g_assert(l != NULL);
	g_assert(c != NULL);

	if (l->argc < 2) {
		return FALSE;
	}

	/* Only optimize easy queries... */
	if (strchr(l->args[1], ',')) {
		return FALSE;
	}

	/* Queries in the form of MODE #channel mode */
	if (l->argc == 3) {
		g_assert(net != NULL);

		ch = find_channel(net, l->args[1]);
		if (!ch) {
			return FALSE;
		}

		for (i = 0; (m = l->args[2][i]); i++) {
			switch (m) {
			case 'b': client_send_banlist(c, ch); break;
			default: return FALSE;
			}
		}

		return TRUE;
	/* Queries in the form MODE #channel */
	} else if (l->argc == 2) {
		ch = find_channel(net, l->args[1]);
		if (!ch) {
			return FALSE;
		}

		if (!ch->mode_received) {
		    return FALSE;
		}

		client_send_channel_mode(c, ch);

		return TRUE;
	}

	return FALSE;
}

static gboolean client_try_cache_topic(struct irc_client *c,
				       struct irc_network_state *net,
				       struct irc_line *l,
				       const struct cache_settings *settings)
{
	struct irc_channel_state *ch;

	if (l->argc < 2) {
		return FALSE;
	}

	/* Only optimize easy queries... */
	if (strchr(l->args[1], ',')) {
		return FALSE;
	}

	/* No set requests */
	if (l->args[2] != NULL) {
		return FALSE;
	}

	ch = find_channel(net, l->args[1]);
	if (!ch) {
		return FALSE;
	}

	client_send_topic(c, ch, TRUE);

	return TRUE;
}

static gboolean client_try_cache_userhost(struct irc_client *c, struct irc_network_state *net, struct irc_line *l, const struct cache_settings *settings)
{
	/* TODO */
	return FALSE;
}

static gboolean client_try_cache_who(struct irc_client *c, struct irc_network_state *net, struct irc_line *l, const struct cache_settings *settings)
{
	struct irc_channel_state *ch;
	int max_who_age;

	time_t now;
	GList *gl;

	g_assert(l != NULL);
	g_assert(c != NULL);

	if (l->argc < 2) {
		return FALSE;
	}

	/* Only optimize easy queries... */
	if (strchr(l->args[1], ',')) {
		return FALSE;
	}
	if (strchr(l->args[1], '*')) {
		return FALSE;
	}

	/* Don't cache complex requests.. for now */
	if (l->argc > 2) {
		return FALSE;
	}

	max_who_age = settings->max_who_age;

	/* Never cache when max_who_age is set to 0 */
	if (max_who_age == 0) {
		return FALSE;
	}

	if (l->argc != 2) {
		return FALSE;
	}

	g_assert(net != NULL);

	ch = find_channel(net, l->args[1]);
	if (ch == NULL) {
		return FALSE;
	}

	now = time(NULL);

	/* Check that the cache data hasn't expired yet */
	for (gl = ch->nicks; gl; gl = gl->next) {
		struct channel_nick *cn = gl->data;

		if (cn->last_update == 0) {
			return FALSE;
		}

		if ((cn->last_update + max_who_age) <= now) {
			return FALSE;
		}
	}

	for (gl = ch->nicks; gl; gl = gl->next) {
		struct channel_nick *cn = gl->data;
		struct network_nick *nn = cn->global_nick;
		char *info = g_strdup_printf("%d %s", nn->hops, nn->fullname);

		client_send_response(c, RPL_WHOREPLY, l->args[1], nn->username,
				     nn->hostname, nn->server, nn->nick,
				     cn->last_flags, info, NULL);

		g_free(info);
	}

	client_send_response(c, RPL_ENDOFWHO, l->args[1], "End of /WHO list.", NULL);

	return TRUE;
}

static gboolean client_try_cache_names(struct irc_client *c,
				       struct irc_network_state *net,
				       struct irc_line *l,
				       const struct cache_settings *settings)
{
	struct irc_channel_state *ch;

	g_assert(l != NULL);
	g_assert(c != NULL);

	if (l->argc < 2) {
		return FALSE;
	}

	/* Only optimize easy queries... */
	if (strchr(l->args[1], ',')) {
		return FALSE;
	}

	if (l->argc != 2) {
		return FALSE;
	}

	g_assert(net != NULL);

	ch = find_channel(net, l->args[1]);
	if (!ch) {
		return FALSE;
	}

	client_send_nameslist(c, ch);

	return TRUE;
}

/**
 * Table of commands of which the result can be cached.
 */
struct cache_command {
	const char *name;
	/* Should return FALSE if command couldn't be cached */
	gboolean (*try_cache) (struct irc_client *c,
			       struct irc_network_state *net,
			       struct irc_line *l,
			       const struct cache_settings *settings);
} cache_commands[] = {
	{ "MODE", client_try_cache_mode },
	{ "NAMES", client_try_cache_names },
	{ "TOPIC", client_try_cache_topic },
	{ "WHO", client_try_cache_who },
	{ "USERHOST", client_try_cache_userhost },
	{ NULL, NULL }
};

/* Try to answer a client query from cache */
gboolean client_try_cache(struct irc_client *c, struct irc_network_state *net,
			  struct irc_line *l,
			  const struct cache_settings *settings)
{
	g_assert(l != NULL);
	int i;

	if (l->argc == 0)
		return TRUE;

	for (i = 0; cache_commands[i].name; i++) {
		if (!base_strcmp(l->args[0], cache_commands[i].name)) {
			return cache_commands[i].try_cache(c, net, l, settings);
		}
	}

	return FALSE;
}
