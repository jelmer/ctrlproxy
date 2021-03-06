/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2007 Jelmer Vernooĳ <jelmer@jelmer.uk>

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

#include <netinet/in.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>

#include "internals.h"
#include "irc.h"

/* Linked list of clients currently connected (and authenticated, but still
 * need to send USER and NICK commands) */
static GList *pending_clients = NULL;
static gboolean process_from_pending_client(
	struct irc_client *client, const struct irc_line *l);

void client_log(enum log_level l, const struct irc_client *client,
		const char *fmt, ...)
{
	char *ret;
	va_list ap;

	if (client->callbacks->log_fn == NULL) {
		return;
	}

	g_assert(client != NULL);
	g_assert(fmt != NULL);

	va_start(ap, fmt);
	ret = g_strdup_vprintf(fmt, ap);
	va_end(ap);

	client->callbacks->log_fn(l, client, ret);

	g_free(ret);
}

static void on_transport_disconnect(struct irc_transport *transport)
{
	struct irc_client *c = transport->userdata;
	client_disconnect(c, "Hangup from client");
}

static gboolean on_transport_error(
	struct irc_transport *transport, const char *message)
{
	struct irc_client *client = transport->userdata;
	client_disconnect(client, message?message:"Unknown error");
	return FALSE;
}

static void on_transport_charset_error(struct irc_transport *transport,
				       const char *error_msg)
{
	struct irc_client *client = transport->userdata;
	client_send_response(client, ERR_BADCHARENCODING,
		"*", transport->charset, error_msg, NULL);
}

static void transport_log(struct irc_transport *transport,
			  const struct irc_line *l, const GError *error)
{
	struct irc_client *c = transport->userdata;
	client_log(LOG_WARNING, c,
		   "Error sending line '%s': %s", l->args[0],
		   error->message);
}

/**
 * Send a response to a client.
 * @param c Client to send to
 * @param response Response number to send
 */
gboolean client_send_response(struct irc_client *c, int response, ...)
{
	struct irc_line *l;
	gboolean ret;
	va_list ap;

	g_assert(c != NULL);
	g_assert(response > 0);

	va_start(ap, response);
	l = virc_parse_response(c->default_origin,
				client_get_default_target(c), response, ap);
	va_end(ap);

	ret = client_send_line(c, l, NULL);

	free_line(l);

	return ret;
}

/**
 * Build a line and send it to a client
 * @param c Client to send to
 * @param hm Hostmask to use
 */
gboolean client_send_args_ex(struct irc_client *c, const char *hm, ...)
{
	struct irc_line *l;
	gboolean ret;
	va_list ap;

	g_assert(c != NULL);

	va_start(ap, hm);
	l = virc_parse_line(hm, ap);
	va_end(ap);

	ret = client_send_line(c, l, NULL);

	free_line(l);

	return ret;
}

/**
 * Send a message to a client.
 * @param c Client to send to
 * @return whether the line was send correctly
 */
gboolean client_send_args(struct irc_client *c, ...)
{
	struct irc_line *l;
	gboolean ret;
	va_list ap;

	g_assert(c);

	va_start(ap, c);
	l = virc_parse_line(c->default_origin, ap);
	va_end(ap);

	ret = client_send_line(c, l, NULL);

	free_line(l);

	return ret;
}

/**
 * Send a line to a client.
 * @param c Client to send to
 * @param l Line to send
 * @return Whether the line was sent successfully
 */
gboolean client_send_line(struct irc_client *c, const struct irc_line *l, GError **error)
{
	if (c->connected == FALSE) {
		g_set_error_literal(error, IRC_CLIENT_ERROR, IRC_CLIENT_ERROR_DISCONNECTED,
				    "Not connected.");
		return FALSE;
	}

	g_assert(c != NULL);
	g_assert(l != NULL);

	if (c->callbacks->process_to_client != NULL) {
		c->callbacks->process_to_client(c, l);
	}

	state_handle_data(c->state, l);

	return transport_send_line(c->transport, l, error);
}

/*
 * Disconnect a client.
 *
 * @param c Client to disconnect.
 * @param reason Reason of the disconnect. Can be NULL.
 */
void client_disconnect(struct irc_client *c, const char *reason)
{
	g_assert(c != NULL);
	if (c->connected == FALSE) {
		return;
	}
	c->connected = FALSE;
	g_source_remove(c->ping_id);

	pending_clients = g_list_remove(pending_clients, c);

	if (c->callbacks->disconnect != NULL) {
		c->callbacks->disconnect(c);
	}

	client_log(LOG_INFO, c, "Removed client");

	transport_send_args(c->transport, NULL, "ERROR", reason, NULL);

	irc_transport_disconnect(c->transport);

	if (c->exit_on_close) {
		exit(0);
	}
}

static void free_client(struct irc_client *c)
{
	g_assert(c->connected == FALSE);
	g_free(c->description);
	free_network_state(c->state);
	free_login_details(c->login_details);
	g_free(c->client_hostname);
	free_irc_transport(c->transport);
	g_free(c->default_origin);
	if (c->callbacks->free_private_data) {
		c->callbacks->free_private_data(c);
	}
	g_free(c);
}

/**
 * Send the Message of the Day to the client.
 *
 * @param c Client to send to.
 */
gboolean client_send_motd(struct irc_client *c, char **lines)
{
	int i;
	g_assert(c);

	if (lines == NULL) {
		return client_send_response(c, ERR_NOMOTD, "No MOTD file", NULL);
	}

	if (!client_send_response(c, RPL_MOTDSTART, "Start of MOTD", NULL)) {
		return FALSE;
	}
	for(i = 0; lines[i]; i++) {
		if (!client_send_response(c, RPL_MOTD, lines[i], NULL)) {
			return FALSE;
		}
	}
	return client_send_response(c, RPL_ENDOFMOTD, "End of MOTD", NULL);
}

static gboolean on_transport_receive_line(struct irc_transport *transport,
					  const struct irc_line *l)
{
	struct irc_client *client = transport->userdata;

	/* Silently drop empty messages */
	if (l == NULL || l->argc == 0) {
		return TRUE;
	}

	if (client->authenticated) {
		return client->callbacks->process_from_client(client, l);
	}

	if (!process_from_pending_client(client, l)) {
		return FALSE;
	}

	if (client->login_details->nick != NULL &&
		client->login_details->username != NULL &&
		client->state == NULL) {
		client->state = network_state_init(
			client->login_details->nick,
			client->login_details->username,
			client->client_hostname);
	}

	if (client->state != NULL) {
		pending_clients = g_list_remove(pending_clients, client);

		if (!client->callbacks->welcome(client)) {
			return FALSE;
		}

		client->authenticated = TRUE;
		client_log(LOG_INFO, client, "New client");
	}

	return TRUE;
}

static gboolean process_from_pending_client(struct irc_client *client,
					    const struct irc_line *l)
{
	if (!base_strcmp(l->args[0], "NICK")) {
		if (l->argc < 2) {
			client_send_response(client, ERR_NEEDMOREPARAMS,
					     l->args[0], "Not enough parameters", NULL);
			return TRUE;
		}

		client->login_details->nick = g_strdup(l->args[1]);
	} else if (!base_strcmp(l->args[0], "USER")) {

		if (l->argc < 5) {
			client_send_response(client, ERR_NEEDMOREPARAMS,
					      l->args[0], "Not enough parameters", NULL);
			return TRUE;
		}

		client->login_details->username = g_strdup(l->args[1]);
		client->login_details->mode = g_strdup(l->args[2]);
		client->login_details->unused = g_strdup(l->args[3]);
		client->login_details->realname = g_strdup(l->args[4]);
	} else if (!base_strcmp(l->args[0], "PASS")) {
		client->login_details->password = g_strdup(l->args[1]);
	} else if (!base_strcmp(l->args[0], "CONNECT")) {
		if (l->argc < 2) {
			client_send_response(client, ERR_NEEDMOREPARAMS,
					     l->args[0], "Not enough parameters", NULL);
			return TRUE;
		}

		client->callbacks->on_connect(client, l->args[1], l->args[2]?atoi(l->args[2]):6667);
	} else {
		client_send_response(client, ERR_NOTREGISTERED,
			"Register first",
			client->default_origin, NULL);
	}

	return TRUE;
}

/**
 * Ping a client.
 *
 * @param client Client to send ping to
 *
 * @return Whether to keep event around
 */
static gboolean client_ping(struct irc_client *client)
{
	const char *name;
	g_assert(client != NULL);

	client->last_ping = time(NULL);

	if (client->state != NULL && client->state->info->name != NULL) {
		name = client->state->info->name;
	} else {
		name = "PinglCtrlproxy";
	}

	return client_send_args_ex(client, NULL, "PING", name, NULL);
}

const struct irc_transport_callbacks client_transport_callbacks = {
	.log = transport_log,
	.disconnect = on_transport_disconnect,
	.recv = on_transport_receive_line,
	.charset_error = on_transport_charset_error,
	.error = on_transport_error,
	.hangup = irc_transport_disconnect,
};

void free_login_details(struct irc_login_details *details)
{
	g_free(details->nick);
	g_free(details->password);
	g_free(details->realname);
	g_free(details->unused);
	g_free(details->username);
	g_free(details->mode);
	g_free(details);
}

/*
 * @param desc Description of the client
 */
struct irc_client *irc_client_new(struct irc_transport *transport,
				  const char *default_origin, const char *desc,
				  const struct irc_client_callbacks *callbacks)
{
	struct irc_client *client;

	g_assert(transport != NULL);
	g_assert(desc != NULL);

	client = g_new0(struct irc_client, 1);
	if (client == NULL) {
		return NULL;
	}
	client->references = 1;

	client->login_details = g_new0(struct irc_login_details, 1);
	client->client_hostname = transport_get_peer_hostname(transport);
	if (client->client_hostname == NULL) {
		client->client_hostname = g_strdup("UNKNOWN");
	}

	client->default_origin = g_strdup(default_origin);
	client->callbacks = callbacks;

	client->connect_time = time(NULL);
	client->ping_id = g_timeout_add(1000 * 300, (GSourceFunc)client_ping,
					client);
	client->transport = transport;
	irc_transport_set_callbacks(client->transport,
				    &client_transport_callbacks, client);
	client->description = g_strdup(desc);

	if (!client_set_charset(client, DEFAULT_CLIENT_CHARSET)) {
		free_client(client);
		return NULL;
	}

	client->connected = TRUE;
	pending_clients = g_list_append(pending_clients, client);
	return client;
}

/**
 * Kill all current pending clients (not authenticated yet).
 *
 * @param reason Reason to report to the clients for the disconnect.
 */
void kill_pending_clients(const char *reason)
{
	while (pending_clients != NULL) {
		client_disconnect(pending_clients->data, reason);
	}
}

/**
 * Change the character set used to send data to a client
 * @param c client to change the character set for
 * @param name name of the character set to change to
 * @return whether changing the character set succeeded
 */
gboolean client_set_charset(struct irc_client *c, const char *name)
{
	if (!transport_set_charset(c->transport, name)) {
		client_log(LOG_WARNING, c, "Unable to find charset `%s'", name);
		return FALSE;
	}
	return TRUE;
}

const char *client_get_own_hostmask(struct irc_client *c)
{
	if (c->state == NULL) {
		return NULL;
	}
	return c->state->me.hostmask;
}

const char *client_get_default_target(struct irc_client *c)
{
	if (c->state != NULL && c->state->me.nick != NULL) {
		return c->state->me.nick;
	}

	return "*";
}

void client_ref_void(struct irc_client *c)
{
	client_ref(c);
}

struct irc_client *client_ref(struct irc_client *c)
{
	if (c != NULL) {
		c->references++;
	}
	return c;
}

void client_unref(struct irc_client *c)
{
	if (c == NULL) {
		return;
	}

	c->references--;
	if (c->references == 0) {
		free_client(c);
	}
}


gboolean client_send_channel_state_diff(struct irc_client *client,
					struct irc_channel_state *old_state,
					struct irc_channel_state *new_state)
{
	GList *gl;
	gboolean ret;

	/* Send PART for each user that is only in old_state */
	for (gl = old_state->nicks; gl; gl = gl->next) {
		struct channel_nick *on = gl->data;
		struct channel_nick *nn;

		nn = find_channel_nick_hostmask(new_state, on->global_nick->hostmask);
		if (nn == NULL) {
			if (!client_send_args_ex(client, on->global_nick->hostmask,
					    "PART", new_state->name, NULL)) {
				return FALSE;
			}
		} else {
			if (!client_send_args_ex(client, on->global_nick->hostmask,
					    "NICK", nn->global_nick->nick,
					    NULL)) {
				return FALSE;
			}
		}
	}

	/* Send JOIN for each user that is only in new_state */
	for (gl = new_state->nicks; gl; gl = gl->next) {
		struct channel_nick *nn = gl->data;
		struct channel_nick *on;

		on = find_channel_nick(old_state, nn->global_nick->nick);
		if (on == NULL) {
			if (!client_send_args_ex(client, nn->global_nick->hostmask,
					    "JOIN", nn->channel->name, NULL)) {
				return FALSE;
			}
		}
	}

	/* Send TOPIC if the topic is different */
	if (old_state->topic != new_state->topic && (
		old_state->topic == NULL || new_state->topic == NULL ||
		strcmp(old_state->topic, new_state->topic) != 0)) {
		if (!client_send_args_ex(client, new_state->topic_set_by, "TOPIC",
				    new_state->name, new_state->topic, NULL)) {
			return FALSE;
		}
	}

	/* Send MODE if the mode changed */
	if (memcmp(old_state->modes, new_state->modes,
		   sizeof(gboolean) * MAXMODES) != 0) {
		char *mode = mode2string(new_state->modes);
		/* FIXME: Remove old modes */
		ret = client_send_args(client, "MODE", new_state->name, mode, NULL);
		g_free(mode);
		if (!ret) {
			return FALSE;
		}
	}

	return TRUE;
}

/**
 * Send the diff between the current state to change it to some other state.
 * @param c Client to send to
 * @param state State to send
 * @return Whether the state was sent correctly
 */
gboolean client_send_state_diff(struct irc_client *client,
				struct irc_network_state *old_state,
				struct irc_network_state *new_state)
{
	GList *gl;

	/* Call client_send_channel_state_diff() for each channel that exists
	 * in both states*/
	/* Send PART for each channel that is only in old_state */
	for (gl = old_state->channels; gl; gl = gl->next) {
		struct irc_channel_state *os = gl->data;
		struct irc_channel_state *ns;

		ns = find_channel(new_state, os->name);

		if (ns != NULL) {
			if (!client_send_channel_state_diff(client, os, ns)) {
				return FALSE;
			}
		} else {
			if (!client_send_args_ex(client,
					    client_get_own_hostmask(client),
					    "PART", os->name, NULL)) {
				return FALSE;
			}
		}
	}

	/* Call client_send_channel_state() for each channel that is only
	 * in new_state */
	for (gl = new_state->channels; gl; gl = gl->next) {
		struct irc_channel_state *ns = gl->data;
		struct irc_channel_state *os;

		os = find_channel(old_state, ns->name);
		if (os == NULL) {
			if (!client_send_channel_state(client, ns)) {
				return FALSE;
			}
		}
	}

	return TRUE;
}

/**
 * Send a particular state to a client.
 *
 * @param c Client to send to
 * @param state State to send
 */
gboolean client_send_state(struct irc_client *c, struct irc_network_state *state)
{
	GList *cl;
	struct irc_channel_state *ch;
	char *mode;
	gboolean ret = TRUE;

	if (c->state != NULL && strcmp(state->me.nick, c->state->me.nick) != 0) {
		if (!client_send_args_ex(c, c->state->me.hostmask, "NICK",
				    state->me.nick, NULL)) {
			return FALSE;
		}
	}

	g_assert(c != NULL);
	g_assert(state != NULL);

	client_log(LOG_TRACE, c, "Sending state (%d channels)",
		   g_list_length(state->channels));

	for (cl = state->channels; cl; cl = cl->next) {
		ch = (struct irc_channel_state *)cl->data;

		if (!client_send_channel_state(c, ch)) {
			return FALSE;
		}
	}

	mode = mode2string(state->me.modes);
	if (mode != NULL) {
		ret = client_send_args_ex(c, state->me.nick, "MODE", mode, NULL);
	}
	g_free(mode);

	if (!ret) {
		return FALSE;
	}

	return TRUE;
}

gboolean client_send_channel_state(struct irc_client *c,
			       struct irc_channel_state *ch)
{
	g_assert(c != NULL);
	g_assert(ch != NULL);
	g_assert(ch->name != NULL);

	if (!client_send_args_ex(c, client_get_own_hostmask(c), "JOIN", ch->name,
			    NULL)) {
		return FALSE;
	}

	if (!client_send_topic(c, ch, FALSE)) {
		return FALSE;
	}

	if (!client_send_nameslist(c, ch)) {
		return FALSE;
	}

	return TRUE;
}

gboolean client_send_topic(struct irc_client *c, struct irc_channel_state *ch,
					   gboolean explicit)
{
	gboolean ret;
	if (ch->topic) {
		if (!client_send_response(c, RPL_TOPIC, ch->name, ch->topic, NULL)) {
			return FALSE;
		}
	} else if (explicit) {
		if (!client_send_response(c, RPL_NOTOPIC, ch->name, "No topic set",
								  NULL)) {
			return FALSE;
		}
	}

	if (ch->topic_set_time != 0 && ch->topic_set_by != NULL) {
		char *tmp = g_strdup_printf("%ld", ch->topic_set_time);
		ret = client_send_response(c, RPL_TOPICWHOTIME, ch->name, ch->topic_set_by,
							 tmp, NULL);
		g_free(tmp);
		if (!ret) {
			return FALSE;
		}
	}

	return TRUE;
}

gboolean client_send_nameslist(struct irc_client *c, struct irc_channel_state *ch)
{
	GList *nl;
	gboolean ret;
	struct irc_line *l = NULL;

	g_assert(c != NULL);
	g_assert(ch != NULL);

	for (nl = ch->nicks; nl; nl = nl->next) {
		char mode[2] = { ch->mode, 0 };
		char *arg;
		struct channel_nick *n = (struct channel_nick *)nl->data;
		char prefix;

		g_assert(c->state != NULL && c->state->info != NULL);
		if (n->modes != NULL) {
			prefix = get_prefix_from_modes(c->state->info, n->modes);
		} else {
			prefix = 0;
		}

		if (prefix == 0) {
			arg = g_strdup(n->global_nick->nick);
		} else {
			arg = g_strdup_printf("%c%s", prefix, n->global_nick->nick);
		}

		ret = TRUE;

		if (l == NULL || !line_add_arg(l, arg)) {
			char *tmp;
			if (l != NULL) {
				ret = client_send_line(c, l, NULL);
				free_line(l);
			}

			l = irc_parse_line_args(c->default_origin, "353",
						client_get_default_target(c),
						mode, ch->name, NULL);
			l->has_endcolon = WITHOUT_COLON;
			tmp = g_strdup_printf(":%s", arg);
			g_assert(line_add_arg(l, tmp));
			g_free(tmp);
		}

		g_free(arg);

		if (!ret) {
			return FALSE;
		}
	}

	if (l != NULL) {
		ret = client_send_line(c, l, NULL);
		free_line(l);
		if (!ret) {
			return FALSE;
		}
	}

	return client_send_response(c, RPL_ENDOFNAMES, ch->name, "End of /NAMES list",
			     NULL);
}


/**
 * Send stat to a list of clients.
 *
 * @param clients List of clients
 * @param s State to send
 */
gboolean clients_send_state(GList *clients, struct irc_network_state *s)
{
	GList *gl;

	for (gl = clients; gl; gl = gl->next) {
		struct irc_client *c = gl->data;
		if (!client_send_state(c, s)) {
			return FALSE;
		}
	}

	return TRUE;
}

gboolean client_send_banlist(struct irc_client *client, struct irc_channel_state *channel)
{
	GList *gl;

	g_assert(channel != NULL);
	g_assert(client != NULL);

	for (gl = channel_mode_nicklist(channel, 'b'); gl; gl = gl->next)
	{
		struct nicklist_entry *be = gl->data;
		g_assert(be != NULL);
		if (!client_send_response(client, RPL_BANLIST, channel->name,
				     be->hostmask, NULL)) {
			return FALSE;
		}
	}

	return client_send_response(client, RPL_ENDOFBANLIST, channel->name,
			     "End of channel ban list", NULL);
}

gboolean client_send_channel_mode(struct irc_client *c, struct irc_channel_state *ch)
{
	char *mode;
	mode = mode2string(ch->modes);
	gboolean ret;

	if (mode != NULL) {
		struct irc_line *l;
		int i, j = 0;
		l = g_new0(struct irc_line, 1);
		l->args = g_new0(char *, 260);
		l->origin = g_strdup(c->default_origin);
		l->args[0] = g_strdup_printf("%03d", RPL_CHANNELMODEIS);
		l->args[1] = g_strdup(client_get_default_target(c));
		l->args[2] = g_strdup(ch->name);
		l->args[3] = mode;
		for (i = 0; mode[i]; i++) {
			if (network_chanmode_type(mode[i], c->state->info) == CHANMODE_OPT_SETTING &&
				channel_mode_option(ch, mode[i])) {
				l->args[4+j] = g_strdup(channel_mode_option(ch, mode[i]));
				j++;
			}
			l->args[4+j] = NULL;
		}
		l->argc = 4+j;
		ret = client_send_line(c, l, NULL);
		free_line(l);
		if (!ret) {
			return FALSE;
		}
	}

	if (ch->creation_time > 0) {
		char time[20];
		snprintf(time, sizeof(time), "%lu", ch->creation_time);
		if (!client_send_response(c, RPL_CREATIONTIME, ch->name, time, NULL)) {
			return FALSE;
		}
	}

	return TRUE;
}

gboolean client_send_luserchannels(struct irc_client *client, int num)
{
	char *tmp;
	gboolean ret;
	tmp = g_strdup_printf("%u", num);
	ret = client_send_response(client, RPL_LUSERCHANNELS, tmp,
				     "channels formed", NULL);
	g_free(tmp);
	return ret;
}

gboolean clients_send_netsplit(GList *clients, const char *my_name,
			   const char *lost_server)
{
	GList *gl;
	for (gl = clients; gl; gl = gl->next) {
		struct irc_client *c = gl->data;
		if (!client_send_netsplit(c, my_name, lost_server)) {
			return FALSE;
		}
	}

	return FALSE;
}

gboolean client_send_nick_quit(struct irc_client *c, struct network_nick *gn,
			       const char *reason)
{
	gboolean ret;
	if (gn->hostmask == NULL) {
		/* Make up a fake hostmask. The user hasn't seen the original hostmask
		 * of this nick anyway, otherwise we would've known it already.
		 */
		char *fake_hostmask = g_strdup_printf("%s!~UNKNOWN@UNKNOWN",
						      gn->nick);
		ret = client_send_args_ex(c, fake_hostmask, "QUIT", reason,
					  NULL);
		g_free(fake_hostmask);
	} else {
		ret = client_send_args_ex(c, gn->hostmask, "QUIT", reason,
					  NULL);
	}
	return ret;
}

gboolean client_send_netsplit(struct irc_client *c, const char *my_name,
			  const char *lost_server)
{
	struct irc_network_state *s = c->state;
	char *reason;
	gboolean ret = TRUE;

	if (s == NULL) {
		return FALSE;
	}

	/* Spoof a netsplit */
	reason = g_strdup_printf("%s %s", my_name, lost_server);

	while (s->nicks != NULL) {
		GList *gl = s->nicks;
		struct network_nick *gn = gl->data;

		/* Skip self */
		if (gn == &s->me) {
			gl = gl->next;
			if (gl == NULL)
				break;
			gn = gl->data;
		}
		ret = client_send_nick_quit(c, gn, reason);
		if (!ret) {
			break;
		}
	}

	g_free(reason);

	return ret;
}

GQuark irc_client_error_quark (void)
{
  return g_quark_from_static_string ("irc-client-error-quark");
}

