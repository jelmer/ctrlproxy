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

#include "internals.h"

static struct channel_config *config_find_channel(struct network_info *ni, struct network_config *nc, const char *name)
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

void network_update_config(struct network_state *ns, struct network_config *nc)
{
	GList *gl;

	nc->autoconnect = 1;

	if (ns == NULL)
		return;
	
	/* Network name */
	if (ns->info != NULL && ns->info->name != NULL && 
		(nc->name == NULL || strcmp(ns->info->name, nc->name) != 0)) {
		g_free(nc->name);
		nc->name = g_strdup(ns->info->name);
	}
	
	/* nick */
	g_free(nc->nick);
	nc->nick = g_strdup(ns->me.nick);

	for (gl = nc->channels; gl; gl = gl->next) {
		struct channel_config *cc = gl->data;

		cc->autojoin = 0;
	}

	for (gl = ns->channels; gl; gl = gl->next) {
		struct channel_state *cs = gl->data;
		struct channel_config *cc;

		/* Find channel */
		cc = config_find_channel(ns->info, nc, cs->name);
		if (!cc) {
			cc = g_new0(struct channel_config, 1);
			nc->channels = g_list_append(nc->channels, cc);
		}
		channel_update_config(cs, cc);
	}

}

void channel_update_config(struct channel_state *ns, struct channel_config *nc)
{
	g_free(nc->key); nc->key = NULL;
	if (ns->key) 
		nc->key = g_strdup(ns->key);
	nc->autojoin = 1;
	g_free(nc->name);
	nc->name = g_strdup(ns->name);
}

void global_update_config(struct global *my_global)
{
	GList *gl;
	for (gl = my_global->networks; gl; gl = gl->next) {
		struct network *n = gl->data;
		network_update_config(n->state, n->config);
	}
}
