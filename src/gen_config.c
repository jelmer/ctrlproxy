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

#include "internals.h"

static struct channel_config *config_find_channel(struct irc_network_info *ni, struct network_config *nc, const char *name)
{
	GList *gl;

	g_assert(ni);
	g_assert(nc);
	g_assert(name);
	
	for (gl = nc->channels; gl; gl = gl->next) {
		struct channel_config *cc = gl->data;

		g_assert(cc->name);

		if (!irccmp(ni, cc->name, name))
			return cc;
	}

	return NULL;
}

void network_update_config(struct irc_network_state *ns, struct network_config *nc,
						   struct ctrlproxy_config *cfg)
{
	GList *gl;

	if (ns == NULL)
		return;
	
	/* nick */
	if (cfg->default_nick == NULL || nc->nick != NULL || strcmp(cfg->default_nick, ns->me.nick) != 0) {
		g_free(nc->nick);
		nc->nick = g_strdup(ns->me.nick);
	}

	for (gl = nc->channels; gl; gl = gl->next) {
		struct channel_config *cc = gl->data;

		cc->autojoin = FALSE;
	}

	for (gl = ns->channels; gl; gl = gl->next) {
		struct irc_channel_state *cs = gl->data;
		struct channel_config *cc;

		/* Find channel */
		cc = config_find_channel(ns->info, nc, cs->name);
		if (!cc) {
			cc = g_new0(struct channel_config, 1);
			cc->name = g_strdup(cs->name);
			nc->channels = g_list_append(nc->channels, cc);
		}
		g_free(cc->key);
		cc->key = NULL;
		if (cs->chanmode_option['k'])
			cc->key = g_strdup(cs->chanmode_option['k']);
		cc->autojoin = TRUE;
	}
}

void global_update_config(struct global *my_global)
{
	GList *gl;
	for (gl = my_global->networks; gl; gl = gl->next) {
		struct irc_network *n = gl->data;
		struct network_config *nc = n->private_data;
	
		nc->autoconnect = (n->connection.state != NETWORK_CONNECTION_STATE_NOT_CONNECTED);

		/* Network name */
		if (n->name != NULL &&
			(nc->name == NULL || strcmp(n->name, nc->name) != 0)) {
			g_free(nc->name);
			nc->name = g_strdup(n->name);
		}

		if (n->connection.state == NETWORK_CONNECTION_STATE_MOTD_RECVD)
			network_update_config(n->external_state, nc, my_global->config);
	}
}
