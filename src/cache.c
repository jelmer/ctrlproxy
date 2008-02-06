/*
	ctrlproxy: A modular IRC proxy
	(c) 2005 Jelmer Vernooij <jelmer@nl.linux.org>

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

void client_send_nameslist(struct irc_client *c, struct irc_network *net, 
						   struct channel_state *ch)
{
	GList *nl;
	struct irc_line *l = NULL;

	g_assert(c != NULL);
	g_assert(ch != NULL);
	
	for (nl = ch->nicks; nl; nl = nl->next) {
		char mode[2] = { ch->mode, 0 };
		char *arg;
		struct channel_nick *n = (struct channel_nick *)nl->data;
		char prefix;

		if (n->modes != NULL) {
			prefix = get_prefix_from_modes(&c->network->info, n->modes);
		} else {
			prefix = 0;
		}

		if (prefix == 0) {
			arg = g_strdup(n->global_nick->nick);
		} else {
			arg = g_strdup_printf("%c%s", prefix, n->global_nick->nick);
		}

		if (l == NULL || !line_add_arg(l, arg)) {
			char *tmp;
			if (l != NULL) {
				client_send_line(c, l);
				free_line(l);
			}

			l = irc_parse_line_args(client_get_default_origin(c), "353",
									client_get_default_target(c), mode, 
									ch->name, NULL);
			l->has_endcolon = WITHOUT_COLON;
			tmp = g_strdup_printf(":%s", arg);
			g_assert(line_add_arg(l, tmp));
			g_free(tmp);
		}

		g_free(arg);
	}

	if (l != NULL) {
		client_send_line(c, l);
		free_line(l);
	}

	client_send_response(c, RPL_ENDOFNAMES, ch->name, "End of /NAMES list", 
						 NULL);
}


static void client_send_banlist(struct irc_client *client, struct irc_network *net, struct channel_state *channel)
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

static gboolean client_try_cache_mode(struct irc_client *c, struct irc_network *net, struct irc_line *l)
{
	int i;
	char m;
	struct channel_state *ch;

	g_assert(l);
	g_assert(c);

	if (l->argc < 2) return FALSE;
	
	/* Only optimize easy queries... */
	if (strchr(l->args[1], ',')) return FALSE;
		
	/* Queries in the form of MODE #channel mode */
	if (l->argc == 3) {
		g_assert(c->network);
		g_assert(c->network->state);

		ch = find_channel(c->network->state, l->args[1]);
		if (!ch) return FALSE;

		for (i = 0; (m = l->args[2][i]); i++) {
			switch (m) {
			case 'b': client_send_banlist(c, net, ch); break;
			default: return FALSE;
			}
		}

		return TRUE;
	/* Queries in the form MODE #channel */
	} else if (l->argc == 2) {
		char *mode;
		ch = find_channel(c->network->state, l->args[1]);
		if (!ch) return FALSE;

		if (!ch->mode_received) return FALSE;

		mode = mode2string(ch->modes);
		client_send_response(c, RPL_CHANNELMODEIS, ch->name, mode, NULL);
		g_free(mode);
		if (ch->creation_time > 0) {
			char time[20];
			snprintf(time, sizeof(time), "%lu", ch->creation_time);
			client_send_response(c, RPL_CREATIONTIME, ch->name, time, NULL);
		}

		return TRUE;
	}

	return FALSE;
}

static gboolean client_try_cache_topic(struct irc_client *c, struct irc_network *net, struct irc_line *l)
{
	struct channel_state *ch;

	if (l->argc < 2) return FALSE;
	
	/* Only optimize easy queries... */
	if (strchr(l->args[1], ',')) return FALSE;

	/* No set requests */
	if (l->args[2]) return FALSE;
	
	ch = find_channel(c->network->state, l->args[1]);
	if (!ch) return FALSE;

	if (ch->topic) {
		client_send_response(c, RPL_TOPIC, ch->name, ch->topic, NULL);
	} else {
		client_send_response(c, RPL_NOTOPIC, ch->name, "No topic set", NULL);
	}

	return TRUE;
}

static gboolean client_try_cache_userhost(struct irc_client *c, struct irc_network *net, struct irc_line *l)
{
	return FALSE;
}

static gboolean client_try_cache_who(struct irc_client *c, struct irc_network *net, struct irc_line *l)
{
	struct channel_state *ch;
	int max_who_age;
	time_t now;
	GList *gl;

	g_assert(l);
	g_assert(c);

	if (l->argc < 2) return FALSE;
	
	/* Only optimize easy queries... */
	if (strchr(l->args[1], ',')) return FALSE;
	if (strchr(l->args[1], '*')) return FALSE;

	/* Don't cache complex requests.. for now */
	if (l->argc > 2) return FALSE;

	max_who_age = c->network->global->config->max_who_age;

	/* Never cache when max_who_age is set to 0 */
	if (max_who_age == 0)
		return FALSE;

	if (l->argc != 2) 
		return FALSE;

	g_assert(c->network);
	g_assert(c->network->state);

	ch = find_channel(c->network->state, l->args[1]);
	if (ch == NULL)
		return FALSE;

	now = time(NULL);

	/* Check that the cache data hasn't expired yet */
	for (gl = ch->nicks; gl; gl = gl->next) {
		struct channel_nick *cn = gl->data;
		
		if (cn->last_update == 0)
			return FALSE;

		if ((cn->last_update + max_who_age) <= now)
			return FALSE;
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

static gboolean client_try_cache_names(struct irc_client *c, struct irc_network *net, struct irc_line *l)
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

	client_send_nameslist(c, net, ch);

	return TRUE;
}

/**
 * Table of commands of which the result can be cached.
 */
struct cache_command {
	const char *name;
	/* Should return FALSE if command couldn't be cached */
	gboolean (*try_cache) (struct irc_client *c, struct irc_network *net, struct irc_line *l);
} cache_commands[] = {
	{ "MODE", client_try_cache_mode },
	{ "NAMES", client_try_cache_names },
	{ "TOPIC", client_try_cache_topic },
	{ "WHO", client_try_cache_who },
	{ "USERHOST", client_try_cache_userhost },
	{ NULL, NULL }
};

/* Try to answer a client query from cache */
gboolean client_try_cache(struct irc_client *c, struct irc_network *net, struct irc_line *l)
{
	g_assert(l);
	int i;

	if (l->argc == 0) 
		return TRUE;

	for (i = 0; cache_commands[i].name; i++) {
		if (!g_strcasecmp(l->args[0], cache_commands[i].name))
			return cache_commands[i].try_cache(c, net, l);
	}
	
	return FALSE;
}
