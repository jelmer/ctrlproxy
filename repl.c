/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2005 Jelmer Vernooij <jelmer@nl.linux.org>

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

static char *mode2string(char modes[255])
{
	static char ret[255];
	unsigned char i;
	int pos = 0;
	for(i = 0; i < 255; i++) {
		if(modes[i]) { ret[pos] = (char)i; pos++; }
	}
	return ret;
}



static void gen_replication_channel(struct client *c, struct channel_state *ch)
{
	struct channel_nick *n;
	GList *nl;
	irc_sendf(c->incoming, ":%s JOIN %s\r\n", c->network->state->me.hostmask, ch->name);

	if(ch->topic) {
		client_send_response(c, RPL_TOPIC, ch->name, ch->topic, NULL);
	} else {
		client_send_response(c, RPL_NOTOPIC, ch->name, "No topic set", NULL);
	}

	for (nl = ch->nicks; nl; nl = nl->next) {
		char mode[2] = { ch->mode, 0 };
		char *tmp;
		n = (struct channel_nick *)nl->data;
		if(n->mode && n->mode != ' ') {
			tmp = g_strdup_printf("%c%s", n->mode, n->global_nick->nick);
		} else 	{ 
			tmp = g_strdup(n->global_nick->nick);
		}
		client_send_response(c, RPL_NAMREPLY, mode, ch->name, tmp, NULL);
		g_free(tmp);
	}
	client_send_response(c, RPL_ENDOFNAMES, ch->name, "End of /names list", NULL);
}

void client_send_state(struct client *c, struct network_state *state)
{
	GList *cl;
	struct channel_state *ch;
	for (cl = state->channels; cl; cl = cl->next) {
		ch = (struct channel_state *)cl->data;

		gen_replication_channel(c, ch);
	}

	if(strlen(mode2string(state->me.modes)))
		irc_sendf(c->incoming, ":%s MODE %s +%s\r\n", c->network->state->me.hostmask, state->me.nick, mode2string(state->me.modes));
}