/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2007 Jelmer Vernooij <jelmer@nl.linux.org>

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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <netinet/in.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "internals.h"
#include "irc.h"

/* Linked list of clients currently connected (and authenticated, but still 
 * need to send USER and NICK commands) */
static GList *pending_clients = NULL;
static gboolean process_from_pending_client(struct irc_client *client, 
											const struct irc_line *l);

void client_log(enum log_level l, const struct irc_client *client,
				 const char *fmt, ...)
{
	char *ret;
	va_list ap;

	if (client->callbacks->log_fn == NULL)
		return;

	g_assert(client);
	g_assert(fmt);

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

static gboolean on_transport_error(struct irc_transport *transport,
							   const char *message)
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
							  const struct irc_line *l,
							  const GError *error)
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
	l = virc_parse_line(c->default_origin, ap);
	va_end(ap);

	l->args = g_realloc(l->args, sizeof(char *) * (l->argc+4));
	memmove(&l->args[2], &l->args[0], l->argc * sizeof(char *));

	l->args[0] = g_strdup_printf("%03d", response);
	l->args[1] = g_strdup(client_get_default_target(c));

	l->argc+=2;
	l->args[l->argc] = NULL;

	ret = client_send_line(c, l);

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

	g_assert(c);

	va_start(ap, hm);
	l = virc_parse_line(hm, ap);
	va_end(ap);

	ret = client_send_line(c, l);

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

	ret = client_send_line(c, l);

	free_line(l); 

	return ret;
}

/**
 * Send a line to a client.
 * @param c Client to send to
 * @param l Line to send
 * @return Whether the line was sent successfully
 */
gboolean client_send_line(struct irc_client *c, const struct irc_line *l)
{
	if (c->connected == FALSE)
		return FALSE;

	g_assert(c != NULL);
	g_assert(l != NULL);
	if (c->callbacks->process_to_client != NULL)
		c->callbacks->process_to_client(c, l);

	state_handle_data(c->state, l);

	return transport_send_line(c->transport, l);
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
	if (c->connected == FALSE)
		return;
	c->connected = FALSE;
	g_source_remove(c->ping_id);

	if (c->callbacks->disconnect != NULL)
		c->callbacks->disconnect(c);

	pending_clients = g_list_remove(pending_clients, c);

	lose_client_hook_execute(c);

	client_log(LOG_INFO, c, "Removed client");

	transport_send_args(c->transport, NULL, "ERROR", reason);

	irc_transport_disconnect(c->transport);

	if (c->exit_on_close) 
		exit(0);

	client_unref(c);
}

static void free_client(struct irc_client *c)
{
	g_assert(c->connected == FALSE);
	g_free(c->description);
	free_network_state(c->state);
	g_free(c->requested_nick);
	g_free(c->requested_username);
	g_free(c->requested_hostname);
	free_irc_transport(c->transport);
	g_free(c->default_origin);
	if (c->callbacks->free_private_data)
		c->callbacks->free_private_data(c);
	g_free(c);
}

/** 
 * Send the Message of the Day to the client.
 *
 * @param c Client to send to.
 */
void client_send_motd(struct irc_client *c, char **lines)
{
	int i;
	g_assert(c);

	if (lines == NULL) {
		client_send_response(c, ERR_NOMOTD, "No MOTD file", NULL);
		return;
	}

	client_send_response(c, RPL_MOTDSTART, "Start of MOTD", NULL);
	for(i = 0; lines[i]; i++) {
		client_send_response(c, RPL_MOTD, lines[i], NULL);
	}
	client_send_response(c, RPL_ENDOFMOTD, "End of MOTD", NULL);
}

static gboolean on_transport_receive_line(struct irc_transport *transport,
										  const struct irc_line *l)
{
	struct irc_client *client = transport->userdata;

	g_assert(l);

	/* Silently drop empty messages */
	if (l->argc == 0) {
		return TRUE;
	}

	if (client->authorized) {
		return client->callbacks->process_from_client(client, l);
	} else {
		if (!process_from_pending_client(client, l)) {
			return FALSE;
		}

		if (client->requested_nick != NULL &&
			client->requested_username != NULL &&
			client->requested_hostname != NULL && 
			client->state == NULL) {
			client->state = network_state_init(client->requested_nick, 
											   client->requested_username,
											   client->requested_hostname);
		}

		if (client->state != NULL) {
			if (client->network == NULL) {
				client_disconnect(client, 
					  "Please select a network first, or specify one in your configuration file");
				return FALSE;
			}

			if (!client->callbacks->welcome(client)) {
				return FALSE;
			}

			pending_clients = g_list_remove(pending_clients, client);
			client->network->clients = g_list_append(client->network->clients, client);

			client->authorized = TRUE;

			client_log(LOG_INFO, client, "New client");

		}
	}
	return TRUE;
}

static gboolean process_from_pending_client(struct irc_client *client, 
											const struct irc_line *l)
{
	extern struct global *my_global;

	if (!g_strcasecmp(l->args[0], "NICK")) {
		if (l->argc < 2) {
			client_send_response(client, ERR_NEEDMOREPARAMS,
						 l->args[0], "Not enough parameters", NULL);
			return TRUE;
		}

		client->requested_nick = g_strdup(l->args[1]);
	} else if (!g_strcasecmp(l->args[0], "USER")) {

		if (l->argc < 5) {
			client_send_response(client, ERR_NEEDMOREPARAMS, 
							 l->args[0], "Not enough parameters", NULL);
			return TRUE;
		}

		client->requested_username = g_strdup(l->args[1]);
		client->requested_hostname = g_strdup(l->args[2]);
	} else if (!g_strcasecmp(l->args[0], "PASS")) {
		/* Silently drop... */
	} else if (!g_strcasecmp(l->args[0], "CONNECT")) {
		if (l->argc < 2) {
			client_send_response(client, ERR_NEEDMOREPARAMS,
							 l->args[0], "Not enough parameters", NULL);
			return TRUE;
		}

		client->network = network_ref(find_network_by_hostname(my_global, 
				l->args[1], l->args[2]?atoi(l->args[2]):6667, TRUE));

		if (client->network == NULL) {
			client_log(LOG_ERROR, client, 
				"Unable to connect to network with name %s", 
				l->args[1]);
		}

		if (client->network->connection.state == NETWORK_CONNECTION_STATE_NOT_CONNECTED) {
			client_send_args(client, "NOTICE", 
								client_get_default_target(client),
								"Connecting to network", NULL);
			connect_network(client->network);
		}
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
	g_assert(client != NULL);

	client->last_ping = time(NULL);
	client_send_args_ex(client, NULL, "PING", client->network->info->name, NULL);

	return TRUE;
}

/* GIOChannels passed into this function 
 * should preferably:
 *  - have no encoding set
 *  - work asynchronously
 *
 * @param c Channel to talk over
 * @param desc Description of the client
 */
struct irc_client *irc_client_new(GIOChannel *c, const char *default_origin, const char *desc, struct irc_client_callbacks *callbacks)
{
	struct irc_client *client;

	g_assert(c != NULL);
	g_assert(desc != NULL);

	client = g_new0(struct irc_client, 1);
	g_assert(client != NULL);
	client->references = 1;

	client->default_origin = g_strdup(default_origin);
	client->callbacks = callbacks;

	g_io_channel_set_flags(c, G_IO_FLAG_NONBLOCK, NULL);
	g_io_channel_set_close_on_unref(c, TRUE);
	client->connect_time = time(NULL);
	client->ping_id = g_timeout_add(1000 * 300, (GSourceFunc)client_ping, 
									client);
	client->transport = irc_transport_new_iochannel(c, transport_log, 
													on_transport_disconnect,
													on_transport_receive_line,
													on_transport_charset_error,
													on_transport_error,
													client);
	client->description = g_strdup(desc);
	client->connected = TRUE;

	client_set_charset(client, DEFAULT_CLIENT_CHARSET);
	pending_clients = g_list_append(pending_clients, client);
	return client;
}

void client_parse_buffer(struct irc_client *client)
{
	handle_pending_client_receive(client->transport->incoming, 
			  g_io_channel_get_buffer_condition(client->transport->incoming),
			  client);
}

/**
 * Kill all current pending clients (not authenticated yet).
 *
 * @param reason Reason to report to the clients for the disconnect.
 */
void kill_pending_clients(const char *reason)
{
	while (pending_clients != NULL) 
		client_disconnect(pending_clients->data, reason);
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
	return c->state->me.hostmask;
}

const char *client_get_default_target(struct irc_client *c)
{
	if (c->state != NULL && c->state->me.nick != NULL) 
		return c->state->me.nick;
	
	return "*";
}

struct irc_client *client_ref(struct irc_client *c) 
{
	if (c != NULL)
		c->references++;
	return c;
}

void client_unref(struct irc_client *c) 
{
	if (c == NULL)
		return;

	c->references--;
	if (c->references == 0)
		free_client(c);
}


gboolean client_send_channel_state_diff(struct irc_client *client, 
										struct irc_channel_state *old_state,
										struct irc_channel_state *new_state)
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
								on->channel->name, NULL);
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
gboolean client_send_state_diff(struct irc_client *client, struct irc_network_state *old_state, struct irc_network_state *new_state)
{
	GList *gl;

	/* Call client_send_channel_state_diff() for each channel that exists 
	 * in both states*/
	/* Send PART for each channel that is only in old_state */
	for (gl = old_state->channels; gl; gl = gl->next) {
		struct irc_channel_state *os = gl->data;
		struct irc_channel_state *ns;

		ns = find_channel(new_state, os->name);

		if (ns != NULL)
			client_send_channel_state_diff(client, os, ns);
		else
			client_send_args_ex(client, client_get_own_hostmask(client), 
								"PART", os->name, NULL);
	}

	/* Call client_send_channel_state() for each channel that is only 
	 * in new_state */
	for (gl = new_state->channels; gl; gl = gl->next) {
		struct irc_channel_state *ns = gl->data;
		struct irc_channel_state *os;

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
gboolean client_send_state(struct irc_client *c, struct irc_network_state *state)
{
	GList *cl;
	struct irc_channel_state *ch;
	char *mode;

	if (strcmp(state->me.nick, c->state->me.nick) != 0) {
		client_send_args_ex(c, c->state->me.hostmask, "NICK", state->me.nick, NULL);
	}

	g_assert(c != NULL);
	g_assert(state != NULL);

    client_log(LOG_TRACE, c, "Sending state (%d channels)", 
			   g_list_length(state->channels));

	for (cl = state->channels; cl; cl = cl->next) {
		ch = (struct irc_channel_state *)cl->data;

		client_send_channel_state(c, ch);
	}

	mode = mode2string(state->me.modes);
	if (mode != NULL) 
		client_send_args_ex(c, state->me.nick, "MODE", mode, NULL);
	g_free(mode);

	return TRUE;
}

void client_send_channel_state(struct irc_client *c, 
							   struct irc_channel_state *ch)
{
	g_assert(c != NULL);
	g_assert(ch != NULL);
	g_assert(ch->name != NULL);

	client_send_args_ex(c, client_get_own_hostmask(c), "JOIN", ch->name, 
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

	client_send_nameslist(c, ch);
}

void client_send_topic(struct irc_client *c, struct irc_channel_state *ch)
{
	if (ch->topic) {
		client_send_response(c, RPL_TOPIC, ch->name, ch->topic, NULL);
	} else {
		client_send_response(c, RPL_NOTOPIC, ch->name, "No topic set", NULL);
	}
}

void client_send_nameslist(struct irc_client *c, 
						   struct irc_channel_state *ch)
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
			prefix = get_prefix_from_modes(ch->network->info, n->modes);
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

			l = irc_parse_line_args(c->default_origin, "353",
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


/**
 * Send stat to a list of clients.
 *
 * @param clients List of clients
 * @param s State to send
 */
void clients_send_state(GList *clients, struct irc_network_state *s)
{
	GList *gl;

	for (gl = clients; gl; gl = gl->next) {
		struct irc_client *c = gl->data;
		client_send_state(c, s);
	}
}

void client_send_banlist(struct irc_client *client, struct irc_channel_state *channel)
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

void client_send_channel_mode(struct irc_client *c, struct irc_channel_state *ch)
{
		char *mode;
		mode = mode2string(ch->modes);
		client_send_response(c, RPL_CHANNELMODEIS, ch->name, mode, NULL);
		g_free(mode);
		if (ch->creation_time > 0) {
			char time[20];
			snprintf(time, sizeof(time), "%lu", ch->creation_time);
			client_send_response(c, RPL_CREATIONTIME, ch->name, time, NULL);
		}
}

void client_send_luserchannels(struct irc_client *client, int num)
{
	char *tmp;
	tmp = g_strdup_printf("%u", num);
	client_send_response(client, RPL_LUSERCHANNELS, tmp,
				     "channels formed", NULL);
	g_free(tmp);

}
