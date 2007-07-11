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

static void client_send_channel_state(struct client *c, 
									  struct channel_state *ch)
{
	struct channel_nick *n;
	GList *nl;

	g_assert(c != NULL);
	g_assert(ch != NULL);
	g_assert(c->network != NULL);
	g_assert(c->network->state != NULL);
	g_assert(ch->name != NULL);

	client_send_args_ex(c, c->network->state->me.hostmask, "JOIN", ch->name, 
						NULL);

	if (ch->topic != NULL) {
		client_send_response(c, RPL_TOPIC, ch->name, ch->topic, NULL);
	}
	if (ch->topic_set_time != 0 && ch->topic_set_by != NULL) {
		char *tmp = g_strdup_printf("%lu", ch->topic_set_time);
		client_send_response(c, RPL_TOPICWHOTIME, ch->name, ch->topic_set_by, 
							 tmp, NULL);
		g_free(tmp);
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
	client_send_response(c, RPL_ENDOFNAMES, ch->name, "End of /NAMES list", 
						 NULL);
}

gboolean client_send_channel_state_diff(struct client *client, 
										struct channel_state *old_state,
										struct channel_state *new_state)
{
	GList *gl;

	/* Send PART for each user that is only in old_state */
	for (gl = old_state->nicks; gl; gl = gl->next) {
		struct channel_nick *on = gl->data;
		struct channel_nick *nn;
		
		nn = find_channel_nick_hostmask(new_state, on->global_nick->hostmask);
		if (nn == NULL)
			client_send_args_ex(client, on->global_nick->hostmask, 
								"PART", on->global_nick->nick, NULL);
		else
			client_send_args_ex(client, on->global_nick->hostmask,
								"NICK", nn->global_nick->nick, NULL);
	}

	/* Send JOIN for each user that is only in new_state */
	for (gl = new_state->nicks; gl; gl = gl->next) {
		struct channel_nick *nn = gl->data;
		struct channel_nick *on;
		
		on = find_channel_nick(old_state, nn->global_nick->nick);
		if (on == NULL)
			client_send_args_ex(client, nn->global_nick->hostmask, "JOIN", 
								nn->global_nick->nick, NULL);
	}

	/* Send TOPIC if the topic is different */
	if (strcmp(old_state->topic, new_state->topic) != 0) 
		client_send_args_ex(client, new_state->topic_set_by, "TOPIC", new_state->topic, NULL);

	/* Send MODE if the mode changed */
	if (memcmp(old_state->modes, new_state->modes, 
			   sizeof(old_state->modes)) != 0) {
		char *mode = mode2string(new_state->modes);
		client_send_args(client, "MODE", new_state->name, mode, NULL);
		g_free(mode);
	}

	return TRUE;
}



/**
 * Send the diff between the current state to change it to some other state.
 * @param c Client to send to
 * @param state State to send
 * @return Whether the state was sent correctly
 */
gboolean client_send_state_diff(struct client *client, struct network_state *new_state)
{
	struct network_state *old_state = client->network->state;
	GList *gl;

	/* Call client_send_channel_state_diff() for each channel that exists 
	 * in both states*/
	/* Send PART for each channel that is only in old_state */
	for (gl = old_state->channels; gl; gl = gl->next) {
		struct channel_state *os = gl->data;
		struct channel_state *ns;

		ns = find_channel(new_state, os->name);

		if (ns != NULL)
			client_send_channel_state_diff(client, os, ns);
		else
			client_send_args_ex(client, client->network->state->me.hostmask, 
								"PART", os->name, NULL);
	}

	/* Call client_send_channel_state() for each channel that is only 
	 * in new_state */
	for (gl = new_state->channels; gl; gl = gl->next) {
		struct channel_state *ns = gl->data;
		struct channel_state *os;

		os = find_channel(old_state, ns->name);
		if (os == NULL)
			client_send_channel_state(client, ns);
	}

	return TRUE;
}

/**
 * Send a particular state to a client.
 *
 * @param c Client to send to
 * @param state State to send
 */
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

	g_assert(c != NULL);
	g_assert(state != NULL);

    log_client(LOG_TRACE, c, "Sending state (%d channels)", 
			   g_list_length(state->channels));

	for (cl = state->channels; cl; cl = cl->next) {
		ch = (struct channel_state *)cl->data;

		client_send_channel_state(c, ch);
	}

	mode = mode2string(state->me.modes);
	if (mode != NULL) 
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
			log_client(LOG_WARNING, client, 
					   "Unable to find replication backend '%s'", bn);
			fn = none_replicate;
		}
	}

	fn(client);
}
