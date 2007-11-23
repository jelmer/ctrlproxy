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
static gboolean handle_client_receive(GIOChannel *c, GIOCondition cond, 
									  void *_client);
static gboolean handle_client_send_queue(GIOChannel *c, GIOCondition cond, 
										 void *_client);

/**
 * Process incoming lines from a client.
 *
 * @param c Client to talk to
 * @param l Line received
 * @return Whether the line was processed correctly
 */
static gboolean process_from_client(struct client *c, struct line *l)
{
	g_assert(c != NULL);
	g_assert(l != NULL);

	l->origin = g_strdup(c->state->me.hostmask);

	if (!run_client_filter(c, l, TO_SERVER)) 
		return TRUE;

	g_assert(l->args[0] != NULL);

	if (!g_strcasecmp(l->args[0], "QUIT")) {
		disconnect_client(c, "Client exiting");
		return FALSE;
	} else if (!g_strcasecmp(l->args[0], "PING")) {
		client_send_args(c, "PONG", c->network->info.name, l->args[1], NULL);
	} else if (!g_strcasecmp(l->args[0], "PONG")) {
		if (l->argc < 2) {
			client_send_response(c, ERR_NEEDMOREPARAMS, l->args[0], 
								 "Not enough parameters", NULL);
			return TRUE;
		}
		c->last_pong = time(NULL);
	} else if (!g_strcasecmp(l->args[0], "USER") ||
			  !g_strcasecmp(l->args[0], "PASS")) {
		client_send_response(c, ERR_ALREADYREGISTERED,  
						 "Please register only once per session", NULL);
	} else if (!g_strcasecmp(l->args[0], "CTRLPROXY")) {
		admin_process_command(c, l, 1);
	} else if (c->network->global->config->admin_user != NULL && 
			   !g_strcasecmp(l->args[0], "PRIVMSG") && 
			   !g_strcasecmp(l->args[1], c->network->global->config->admin_user)) {
		admin_process_command(c, l, 2);
	} else if (!g_strcasecmp(l->args[0], "PRIVMSG") && l->argc > 2 && 
			l->args[2][0] == '\001' && 
			g_strncasecmp(l->args[2], "\001ACTION", 7) != 0) {
		ctcp_process_client_request(c, l);
	} else if (!g_strcasecmp(l->args[0], "NOTICE") && l->argc > 2 && 
			l->args[2][0] == '\001') {
		ctcp_process_client_reply(c, l);
	} else if (!g_strcasecmp(l->args[0], "")) {
	} else if (c->network->connection.state == NETWORK_CONNECTION_STATE_MOTD_RECVD) {
		if (c->network->config->disable_cache || !client_try_cache(c, l)) {
			/* Perhaps check for validity of input here ? It could save us some bandwidth 
			 * to the server, though very unlikely to occur often */
			network_send_line(c->network, c, l, FALSE);
		}
	} else if (c->network->connection.state == NETWORK_CONNECTION_STATE_NOT_CONNECTED) {
		char *msg;
		if (c->network->connection.data.tcp.last_disconnect_reason == NULL)
			msg = g_strdup("Currently not connected to server...");
		else
			msg = g_strdup_printf("Currently not connected to server... (%s)",
					c->network->connection.data.tcp.last_disconnect_reason);

		client_send_args(c, "NOTICE", client_get_default_target(c), msg, NULL);
		g_free(msg);
	}

	return TRUE;
}

/**
 * Send a response to a client.
 * @param c Client to send to
 * @param response Response number to send
 */
gboolean client_send_response(struct client *c, int response, ...)
{
	struct line *l;
	gboolean ret;
	va_list ap;

	g_assert(c != NULL);
	g_assert(response > 0);
	
	va_start(ap, response);
	l = virc_parse_line(client_get_default_origin(c), ap);
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
gboolean client_send_args_ex(struct client *c, const char *hm, ...)
{
	struct line *l;
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
gboolean client_send_args(struct client *c, ...)
{
	struct line *l;
	gboolean ret;
	va_list ap;

	g_assert(c);
	
	va_start(ap, c);
	l = virc_parse_line(client_get_default_origin(c), ap);
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
gboolean client_send_line(struct client *c, const struct line *l)
{
	if (c->connected == FALSE)
		return FALSE;

	g_assert(c != NULL);
	g_assert(l != NULL);
	log_client_line(c, l, FALSE);

	state_handle_data(c->state, l);

	if (c->outgoing_id == 0) {
		GError *error = NULL;
		GIOStatus status = irc_send_line(c->incoming, c->outgoing_iconv, l, 
										 &error);

		if (status == G_IO_STATUS_AGAIN) {
			c->outgoing_id = g_io_add_watch(c->incoming, G_IO_OUT, 
											handle_client_send_queue, c);
			g_queue_push_tail(c->pending_lines, linedup(l));
		} else if (status != G_IO_STATUS_NORMAL) {
			c->connected = FALSE;

			log_client(LOG_WARNING, c, "Error sending line '%s': %s", 
							l->args[0], error->message);

			return FALSE;
		} 

		return TRUE;
	}

	g_queue_push_tail(c->pending_lines, linedup(l));

	return TRUE;
}

static void free_pending_line(void *_line, void *userdata)
{
	free_line((struct line *)_line);
}

/*
 * Disconnect a client.
 *
 * @param c Client to disconnect.
 * @param reason Reason of the disconnect. Can be NULL.
 */
void disconnect_client(struct client *c, const char *reason) 
{
	g_assert(c != NULL);
	c->connected = FALSE;
	g_assert(c->incoming != NULL);

	g_io_channel_unref(c->incoming);
	g_source_remove(c->incoming_id);
	if (c->outgoing_id)
		g_source_remove(c->outgoing_id);

	g_source_remove(c->ping_id);

	if (c->network != NULL)
		c->network->clients = g_list_remove(c->network->clients, c);

	pending_clients = g_list_remove(pending_clients, c);

	lose_client_hook_execute(c);

	log_client(LOG_INFO, c, "Removed client");

	irc_send_args(c->incoming, c->outgoing_iconv, NULL, "ERROR", reason, NULL);

	g_io_channel_unref(c->incoming);
	
	c->incoming = NULL;

	g_iconv_close(c->outgoing_iconv);
	g_iconv_close(c->incoming_iconv);

	if (c->exit_on_close) 
		exit(0);

	client_unref(c);
}

static void free_client(struct client *c)
{
	g_assert(c->connected == FALSE);
	g_free(c->charset);
	g_free(c->description);
	free_network_state(c->state);
	g_free(c->requested_nick);
	g_free(c->requested_username);
	g_free(c->requested_hostname);
	g_queue_foreach(c->pending_lines, free_pending_line, NULL);
	g_queue_free(c->pending_lines);
	network_unref(c->network);
	g_free(c);
}

/** 
 * Send the Message of the Day to the client.
 *
 * @param c Client to send to.
 */
void send_motd(struct client *c)
{
	char **lines;
	int i;
	g_assert(c);

	lines = get_motd_lines(c);

	if (!lines) {
		client_send_response(c, ERR_NOMOTD, "No MOTD file", NULL);
		return;
	}

	client_send_response(c, RPL_MOTDSTART, "Start of MOTD", NULL);
	for(i = 0; lines[i]; i++) {
		client_send_response(c, RPL_MOTD, lines[i], NULL);
		g_free(lines[i]);
	}
	g_free(lines);
	client_send_response(c, RPL_ENDOFMOTD, "End of MOTD", NULL);
}

static gboolean handle_client_send_queue(GIOChannel *ioc, GIOCondition cond, 
									  void *_client)
{
	struct client *c = _client;

	g_assert(ioc == c->incoming);

	while (!g_queue_is_empty(c->pending_lines)) {
		GIOStatus status;
		GError *error = NULL;
		struct line *l = g_queue_pop_head(c->pending_lines);

		g_assert(c->incoming != NULL);
		status = irc_send_line(c->incoming, c->outgoing_iconv, l, &error);

		if (status == G_IO_STATUS_AGAIN) {
			g_queue_push_head(c->pending_lines, l);
			return TRUE;
		}

		if (status == G_IO_STATUS_ERROR) {
			log_client(LOG_WARNING, c, "Error sending line '%s': %s", 
							l->args[0], error->message);
		} else if (status == G_IO_STATUS_EOF) {
			c->outgoing_id = 0;
			disconnect_client(c, "Hangup from client");

			free_line(l);

			return FALSE;
		}

		free_line(l);
	}

	c->outgoing_id = 0;
	return FALSE;
}


static gboolean handle_client_receive(GIOChannel *c, GIOCondition cond, 
									  void *_client)
{
	gboolean ret = TRUE;
	struct client *client = (struct client *)_client;
	struct line *l;

	g_assert(client);

	if (cond & G_IO_HUP) {
		disconnect_client(client, "Hangup from client");
		return FALSE;
	}

	if (cond & G_IO_IN) {
		GError *error = NULL;
		GIOStatus status;
		
		while ((status = irc_recv_line(c, client->incoming_iconv, &error, 
									   &l)) == G_IO_STATUS_NORMAL) {
			g_assert(l);

			log_client_line(client, l, TRUE);

			/* Silently drop empty messages */
			if (l->argc == 0) {
				free_line(l);
				continue;
			}

			ret &= process_from_client(client, l);

			free_line(l);

			if (!ret)
				return FALSE;
		}

		if (status == G_IO_STATUS_EOF) {
			disconnect_client(client, "Connection closed by client");
			return FALSE;
		}

		if (status != G_IO_STATUS_AGAIN) {
			if (error->domain == G_CONVERT_ERROR &&
				error->code == G_CONVERT_ERROR_ILLEGAL_SEQUENCE) {
				client_send_response(client, ERR_BADCHARENCODING, 
				   "*", client->charset, error->message, NULL);
			} else {
				disconnect_client(client, error?error->message:"Unknown error");
				return FALSE;
			}
		}
	}

	return ret;
}

/**
 * Send welcome information to a client, optionally disconnecting 
 * the client if it isn't welcome.
 *
 * @param client Client to talk to.
 * @return whether the client was accepted or refused
 */
static gboolean welcome_client(struct client *client)
{
	char *features, *tmp;

	g_assert(client);

	client_send_response(client, RPL_WELCOME, "Welcome to the ctrlproxy", NULL);
	tmp = g_strdup_printf("Host %s is running ctrlproxy", get_my_hostname());
	client_send_response(client, RPL_YOURHOST, tmp, NULL); 
	g_free(tmp);
	client_send_response(client, RPL_CREATED, 
		"Ctrlproxy (c) 2002-2007 Jelmer Vernooij <jelmer@vernstok.nl>", NULL);
	client_send_response(client, RPL_MYINFO, 
		 client->network->info.name, 
		 ctrlproxy_version(), 
		 (client->network->state != NULL && client->network->info.supported_user_modes)?client->network->info.supported_user_modes:ALLMODES, 
		 (client->network->state != NULL && client->network->info.supported_channel_modes)?client->network->info.supported_channel_modes:ALLMODES,
		 NULL);

	features = network_generate_feature_string(client->network);

	client_send_response(client, RPL_BOUNCE, features, 
						 "are supported on this server", NULL);

	g_free(features);

	if (client->network->state != NULL) {
		tmp = g_strdup_printf("%u", g_list_length(client->network->state->channels));
		client_send_response(client, RPL_LUSERCHANNELS, tmp,
				     "channels formed", NULL);
		g_free(tmp);
	}

	tmp = g_strdup_printf("I have %d clients", g_list_length(client->network->clients));
	client_send_response(client, RPL_LUSERME, tmp, NULL);
	g_free(tmp);

	send_motd(client);

	g_assert(client->state != NULL);
	g_assert(client->network != NULL);

	if (client->network->state != NULL) {
		if (g_strcasecmp(client->state->me.nick, client->network->state->me.nick) != 0) {
			/* Tell the client our his/her real nick */
			client_send_args_ex(client, client->state->me.hostmask, "NICK", 
								client->network->state->me.nick, NULL); 

			/* Try to get the nick the client specified */
			if (!client->network->config->ignore_first_nick) {
				network_send_args(client->network, "NICK", 
								  client->requested_nick, NULL);
			}
		}
	}

	if (!new_client_hook_execute(client)) {
		disconnect_client(client, "Refused by client connect hook");
		return FALSE;
	}

	client_replicate(client);

	return TRUE;
}

/**
 * Handles incoming messages from the client
 * @param c IO Channel to receive from
 * @param cond condition that has been triggered
 * @param client pointer to client context
 */
static gboolean handle_pending_client_receive(GIOChannel *c, 
											  GIOCondition cond, void *_client)
{
	struct client *client = (struct client *)_client;
	struct line *l;
	extern struct global *my_global;

	g_assert(client);
	g_assert(c);

	if (cond & G_IO_ERR) {
		char *tmp = g_strdup_printf("Error reading from client: %s", 
						  g_io_channel_unix_get_sock_error(c));
		disconnect_client(client, tmp);
		g_free(tmp);
		return FALSE;
	}

	if (cond & G_IO_HUP) {
		disconnect_client(client, "Hangup from client");
		return FALSE;
	}

	if (cond & G_IO_IN) {
		GError *error = NULL;
		GIOStatus status;
		
		while ((status = irc_recv_line(c, client->incoming_iconv, 
									   &error, &l)) == G_IO_STATUS_NORMAL) 
		{
			g_assert(l);

			/* Silently drop empty messages */
			if (l->argc == 0) {
				free_line(l);
				continue;
			}

			g_assert(l->args[0]);

			if (!g_strcasecmp(l->args[0], "NICK")) {
				if (l->argc < 2) {
					client_send_response(client, ERR_NEEDMOREPARAMS,
								 l->args[0], "Not enough parameters", NULL);
					free_line(l);
					continue;
				}

				client->requested_nick = g_strdup(l->args[1]);
			} else if (!g_strcasecmp(l->args[0], "USER")) {

				if (l->argc < 5) {
					client_send_response(client, ERR_NEEDMOREPARAMS, 
									 l->args[0], "Not enough parameters", NULL);
					free_line(l);
					continue;
				}

				client->requested_username = g_strdup(l->args[1]);
				client->requested_hostname = g_strdup(l->args[2]);
			} else if (!g_strcasecmp(l->args[0], "PASS")) {
				/* Silently drop... */
			} else if (!g_strcasecmp(l->args[0], "CONNECT")) {
				if (l->argc < 2) {
					client_send_response(client, ERR_NEEDMOREPARAMS,
									 l->args[0], "Not enough parameters", NULL);
					free_line(l);
					continue;
				}

				client->network = find_network_by_hostname(my_global, 
						l->args[1], l->args[2]?atoi(l->args[2]):6667, TRUE);

				if (!client->network) {
					log_client(LOG_ERROR, client, 
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
					client_get_default_origin(client), NULL);
			}

			free_line(l);

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
					disconnect_client(client, 
						  "Please select a network first, or specify one in your configuration file");
					return FALSE;
				}

				if (!welcome_client(client)) {
					return FALSE;
				}

				client->incoming_id = g_io_add_watch(client->incoming, 
							 G_IO_IN | G_IO_HUP | G_IO_ERR, handle_client_receive, client);

				pending_clients = g_list_remove(pending_clients, client);
				client->network->clients = g_list_append(client->network->clients, client);
				log_client(LOG_INFO, client, "New client");

				return FALSE;
			}
		}

		if (status != G_IO_STATUS_AGAIN) {
			disconnect_client(client, "Error receiving line from client");
			return FALSE;
		}

		return TRUE;
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
static gboolean client_ping(struct client *client)
{
	g_assert(client != NULL);

	client->last_ping = time(NULL);
	client_send_args_ex(client, NULL, "PING", client->network->info.name, NULL);

	return TRUE;
}


/* GIOChannels passed into this function 
 * should preferably:
 *  - have no encoding set
 *  - work asynchronously
 *
 * @param n Network to use
 * @param c Channel to talk over
 * @param desc Description of the client
 */
struct client *client_init(struct network *n, GIOChannel *c, const char *desc)
{
	struct client *client;
	gboolean charset_ok = FALSE;

	g_assert(c != NULL);
	g_assert(desc != NULL);

	client = g_new0(struct client, 1);
	client->references = 1;
	g_assert(client);

	g_io_channel_set_flags(c, G_IO_FLAG_NONBLOCK, NULL);
	g_io_channel_set_close_on_unref(c, TRUE);
	client->connect_time = time(NULL);
	client->ping_id = g_timeout_add(1000 * 300, (GSourceFunc)client_ping, 
									client);
	client->incoming = c;
	client->network = network_ref(n);
	client->description = g_strdup(desc);
	client->connected = TRUE;
	client->exit_on_close = FALSE;
	client->pending_lines = g_queue_new();

	client->outgoing_iconv = client->incoming_iconv = (GIConv)-1;
	if (n != NULL && n->global != NULL)
		charset_ok = client_set_charset(client, 
										n->global->config->client_charset);
	if (!charset_ok)
		client_set_charset(client, DEFAULT_CLIENT_CHARSET);

	client->incoming_id = g_io_add_watch(client->incoming, G_IO_IN | G_IO_HUP, 
										 handle_pending_client_receive, client);

	handle_pending_client_receive(client->incoming, 
				  g_io_channel_get_buffer_condition(client->incoming),
				  client);

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
	while (pending_clients != NULL) 
		disconnect_client(pending_clients->data, reason);
}

/**
 * Change the character set used to send data to a client
 * @param c client to change the character set for
 * @param name name of the character set to change to
 * @return whether changing the character set succeeded
 */
gboolean client_set_charset(struct client *c, const char *name)
{
	GIConv tmp;

	if (name != NULL) {
		tmp = g_iconv_open(name, "UTF-8");

		if (tmp == (GIConv)-1) {
			log_client(LOG_WARNING, c, "Unable to find charset `%s'", name);
			return FALSE;
		}
	} else {
		tmp = (GIConv)-1;
	}
	
	if (c->outgoing_iconv != (GIConv)-1)
		g_iconv_close(c->outgoing_iconv);

	c->outgoing_iconv = tmp;

	if (name != NULL) {
		tmp = g_iconv_open("UTF-8", name);

		if (tmp == (GIConv)-1) {
			log_client(LOG_WARNING, c, "Unable to find charset `%s'", name);
			return FALSE;
		}
	} else {
		tmp = (GIConv)-1;
	}

	if (c->incoming_iconv != (GIConv)-1)
		g_iconv_close(c->incoming_iconv);

	c->incoming_iconv = tmp;

	g_free(c->charset);
	c->charset = g_strdup(name);

	return TRUE;
}

const char *client_get_own_hostmask(struct client *c)
{
	return c->state->me.hostmask;
}

const char *client_get_default_origin(struct client *c)
{
	return c->network?c->network->info.name:get_my_hostname();
}

const char *client_get_default_target(struct client *c)
{
	if (c->state != NULL && c->state->me.nick != NULL) 
		return c->state->me.nick;
	
	return "*";
}

struct client *client_ref(struct client *c) 
{
	if (c != NULL)
		c->references++;
	return c;
}

void client_unref(struct client *c) 
{
	if (c == NULL)
		return;

	c->references--;
	if (c->references == 0)
		free_client(c);
}
