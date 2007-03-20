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
#include "repl.h"

void string2mode(char *modes, char ar[255])
{
	memset(ar, 0, sizeof(ar));

	if (modes == NULL)
		return;

	g_assert(modes[0] == '+');
	modes++;
	for (; *modes; modes++) {
		ar[(int)(*modes)] = 1;
	}
}

char *mode2string(char modes[255])
{
	char ret[256];
	unsigned char i;
	int pos = 0;
	ret[0] = '\0';
	for(i = 0; i < 255; i++) {
		if(modes[i]) { ret[pos] = (char)i; pos++; }
	}
	ret[pos] = '\0';

	if (strlen(ret) == 0) {
		return NULL;
	} else {
		return g_strdup_printf("+%s", ret);
	}
}

static void gen_replication_channel(struct client *c, struct channel_state *ch)
{
	struct channel_nick *n;
	GList *nl;

	g_assert(c);
	g_assert(ch);
	g_assert(c->network);
	g_assert(c->network->state);
	g_assert(ch->name);

	client_send_args_ex(c, c->network->state->me.hostmask, "JOIN", ch->name, NULL);

	if(ch->topic) {
		client_send_response(c, RPL_TOPIC, ch->name, ch->topic, NULL);
	}
	if(ch->topic_set_time && ch->topic_set_by) {
		char tmp[5];
		snprintf(tmp, sizeof(tmp), "%lu", ch->topic_set_time);
		client_send_response(c, RPL_TOPICWHOTIME, ch->name, ch->topic_set_by, tmp, NULL);
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
	client_send_response(c, RPL_ENDOFNAMES, ch->name, "End of /NAMES list", NULL);
}

gboolean client_send_state(struct client *c, struct network_state *state)
{
	GList *cl;
	struct channel_state *ch;
	char *mode;

	if (strcmp(state->me.nick, c->nick) != 0) {
		client_send_args_ex(c, c->nick, "NICK", state->me.nick, NULL);
		g_free(c->nick);
		c->nick = g_strdup(state->me.nick);
	}

	g_assert(c);
	g_assert(state);

    log_client(LOG_TRACE, c, "Sending state (%d channels)", g_list_length(state->channels));

	for (cl = state->channels; cl; cl = cl->next) {
		ch = (struct channel_state *)cl->data;

		gen_replication_channel(c, ch);
	}

	mode = mode2string(state->me.modes);
	if (mode) 
		client_send_args_ex(c, state->me.nick, "MODE", mode, NULL);
	g_free(mode);

	return TRUE;
}

static void none_replicate(struct client *c)
{
	if (c->network->state)
		client_send_state(c, c->network->state);
}

static GList *backends = NULL;

void register_replication_backend(const struct replication_backend *backend)
{
	backends = g_list_append(backends, g_memdup(backend, sizeof(*backend)));
}

void client_replicate(struct client *client)
{
	void (*fn) (struct client *);
	const char *bn = client->network->global->config->replication;
	
	if (bn == NULL || !strcmp(bn, "none")) 
		fn = none_replicate;
	else {
		GList *gl;
		fn = NULL;

		for (gl = backends; gl; gl = gl->next) {
			struct replication_backend *backend = gl->data;
			if (!strcmp(backend->name, bn))
				fn = backend->replication_fn;
		}

		if (!fn) {
			log_client(LOG_WARNING, client, "Unable to find replication backend '%s'\n", bn);
			fn = none_replicate;
		}
	}

	fn(client);
}
