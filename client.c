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
#define __USE_POSIX
#include <netdb.h>

#include "internals.h"
#include "irc.h"

/* Linked list of clients currently connected (and authenticated, but still need to 
 * send USER and NICK commands) */
static GList *pending_clients = NULL;

static char *network_generate_feature_string(struct network *n)
{
	GList *fs = NULL, *gl;
	char *name, *casemap;
	char *ret;
	
	name = g_strdup_printf("NETWORK=%s", n->name);
	fs = g_list_append(fs, name);

	switch (n->supports.casemapping) {
	default:
	case CASEMAP_RFC1459:
		casemap = g_strdup("CASEMAPPING=rfc1459");
		break;
	case CASEMAP_STRICT_RFC1459:
		casemap = g_strdup("CASEMAPPING=strict-rfc1459");
		break;
	case CASEMAP_ASCII:
		casemap = g_strdup("CASEMAPPING=ascii");
		break;
	}

	fs = g_list_append(fs, casemap);

	fs = g_list_append(fs, g_strdup("FNC"));

	ret = list_make_string(fs);

	for (gl = fs; gl; gl = gl->next)
	{
		g_free(gl->data);
	}

	g_list_free(fs);

	return ret;
}

static void client_send_banlist(struct client *client, struct channel *channel)
{
	GList *gl;
	for (gl = channel->banlist; gl; gl = gl->next)
	{
		struct banlist_entry *be = gl->data;
		client_send_args(client, "367", channel->name, be->hostmask, NULL);
	}

	client_send_args(client, "368", "End of channel ban list", NULL);
}

/* Try to answer a client query from cache */
static gboolean client_try_cache(struct line *l)
{
	if (l->argc == 0) return TRUE;

	if (!g_strcasecmp(l->args[0], "JOIN")) {
		struct channel *c;

		/* Only optimize easy queries :-) */
		if (strchr(l->args[1], ',')) return FALSE;
		
		c = find_channel(l->client->network, l->args[1]);
		return (c && c->joined);
	}
	
	if (!g_strcasecmp(l->args[0], "MODE")) {
		int i;
		char m;
		struct channel *c;

		/* Only optimize easy queries... */
		if (strchr(l->args[1], ',')) return FALSE;
		
		/* Only queries in the form of MODE #channel mode */
		if (l->argc != 3) return FALSE; 

		c = find_channel(l->client->network, l->args[1]);
		if (!c) return FALSE;

		for (i = 0; (m = l->args[2][i]); i++) {
			switch (m) {
			case 'b': client_send_banlist(l->client, c); break;
			default: return FALSE;
			}
		}

		return TRUE;
	}

	return FALSE;
}

static gboolean process_from_client(struct line *l)
{
	l->direction = TO_SERVER;
	l->origin = g_strdup(l->client->network->hostmask);

	if (!run_client_filter(l)) 
		return TRUE;

	if(!g_strcasecmp(l->args[0], "QUIT")) {
		disconnect_client(l->client, "Client exiting");
		return FALSE;
	} else if(!g_strcasecmp(l->args[0], "PING")) {
		client_send_args(l->client, "PONG", l->network->name, l->args[1], NULL);
	} else if(!g_strcasecmp(l->args[0], "PONG")) {
		if (l->argc < 2) {
			client_send_args(l->client, "461", l->client->nick, l->args[0], "Not enough parameters", NULL);
			return TRUE;
		}
		l->client->last_pong = time(NULL);
	} else if(!g_strcasecmp(l->args[0], "USER") ||
			  !g_strcasecmp(l->args[0], "PASS")) {
		client_send_args(l->client, "462", l->client->nick, 
						 "Please register only once per session", NULL);
	} else if(l->client->network->state == NETWORK_STATE_MOTD_RECVD) {
		gboolean from_cache = client_try_cache(l);

		if (!from_cache) {
			redirect_record(l->client, l);
			/* FIXME: Check for validity of input ? */
			network_send_line(l->client->network, l);
		}
	} else if(l->client->network->state == NETWORK_STATE_NOT_CONNECTED) {
		client_send_args(l->client, "NOTICE", l->client->nick, "Currently not connected to server, connecting...", NULL);
		connect_network(l->client->network);
	}

	return TRUE;
}

gboolean client_send_args(struct client *c, ...)
{
	struct line *l;
	gboolean ret;
	va_list ap;

	if(!c) return FALSE;
	
	va_start(ap, c);
	l = virc_parse_line(c->network?c->network->name:"ctrlproxy", ap);
	va_end(ap);

	ret = client_send_line(c, l);

	free_line(l); l = NULL;

	return ret;
}

gboolean client_send_line(struct client *c, const struct line *l)
{
	return irc_send_line(c->incoming, l);
}

void disconnect_client(struct client *c, const char *reason) 
{
	g_assert(c->incoming);

	irc_send_args(c->incoming, "ERROR", reason, NULL);

	g_source_remove(c->incoming_id);
	c->incoming = NULL;

	g_source_remove(c->ping_id);

	if (c->network) {
		c->network->clients = g_list_remove(c->network->clients, c);
	} 

	pending_clients = g_list_remove(pending_clients, c);

	lose_client_hook_execute(c);

	log_client(NULL, c, "Removed client");

	if (c->exit_on_close) 
		exit(0);

	g_free(c->username);
	g_free(c->fullname);
	g_free(c->nick);
	g_free(c);
}

void send_motd(struct client *c)
{
	char **lines;
	int i;
	lines = get_motd_lines(c->network);

	if(!lines) {
		client_send_args(c, "422", c->nick, "No MOTD file", NULL);
		return;
	}

	client_send_args(c, "375", c->nick, "Start of MOTD", NULL);
	for(i = 0; lines[i]; i++) {
		client_send_args(c, "372", c->nick, lines[i], NULL);
		g_free(lines[i]);
	}
	g_free(lines);
	client_send_args(c, "376", c->nick, "End of MOTD", NULL);
}

static gboolean handle_client_receive(GIOChannel *c, GIOCondition cond, void *_client)
{
	gboolean ret;
	struct client *client = (struct client *)_client;
	struct line *l;

	if (cond & G_IO_HUP) {
		disconnect_client(client, "Hangup from client");
		return FALSE;
	}

	if (cond & G_IO_IN) {
		GError *error= NULL;
		GIOStatus status = irc_recv_line(c, &error, &l);

		if (status == G_IO_STATUS_ERROR) {
			disconnect_client(client, error?error->message:"Unknown error");
			return FALSE;
		}

		if(status == G_IO_STATUS_EOF ||
		   status == G_IO_STATUS_AGAIN || 
		   !l) return TRUE;

		/* Silently drop empty messages */
		if (l->argc == 0) {
			free_line(l);
			return TRUE;
		}

		l->client = client;
		l->network = client->network;

		ret = process_from_client(l);

		free_line(l);

		return ret;
	}
	return TRUE;
}

static gboolean welcome_client(struct client *client)
{
	char *features;
	client_send_args(client, "001", client->nick, "Welcome to the ctrlproxy", NULL);
	irc_sendf(client->incoming, ":%s 002 %s :Host %s is running ctrlproxy\r\n", client->network->name, client->nick, get_my_hostname());
	client_send_args(client, "003", client->nick, "Ctrlproxy (c) 2002-2005 Jelmer Vernooij <jelmer@vernstok.nl>", NULL);
	client_send_args(client, "004", 
		 client->nick, 
		 client->network->name, 
		 ctrlproxy_version(), 
		 client->network->supported_modes[0]?client->network->supported_modes[0]:ALLMODES, 
		 client->network->supported_modes[1]?client->network->supported_modes[1]:ALLMODES,
		 NULL);

	features = network_generate_feature_string(client->network);

	client_send_args(client, "005", client->nick, features, "are supported on this server", NULL);

	g_free(features);

	send_motd(client);

	if (g_strcasecmp(client->nick, client->network->nick)) {
		/* Or tell the client our his/her real nick */
		irc_sendf(client->incoming, ":%s!%s@%s NICK %s", client->nick, client->username, client->hostname, client->network->nick);

		/* Try to get the nick the client specified */
		if (!client->network->ignore_first_nick) {
			network_send_args(client->network, "NICK", client->nick, NULL);
		}
	}

	if(!new_client_hook_execute(client)) {
		disconnect_client(client, "Refused by client connect hook");
		return FALSE;
	}

	return TRUE;
}

static gboolean handle_pending_client_receive(GIOChannel *c, GIOCondition cond, void *_client)
{
	struct client *client = (struct client *)_client;
	struct line *l;

	if (cond & G_IO_HUP) {
		disconnect_client(client, "Hangup from client");
		return FALSE;
	}

	if (cond & G_IO_IN) {
		GError *error = NULL;
		GIOStatus status = irc_recv_line(c, &error, &l);
		
		if (status != G_IO_STATUS_NORMAL) {
			disconnect_client(client, "Error receiving line from client");
			return FALSE;
		}

		if(!l) return TRUE;

		/* Silently drop empty messages */
		if (l->argc == 0) {
			free_line(l);
			return TRUE;
		}

		if(!g_strcasecmp(l->args[0], "NICK")) {
			if (l->argc < 2) {
				client_send_args(client, "461", client->nick?client->nick:"*",
								 l->args[0], "Not enough parameters", NULL);
				free_line(l);
				return TRUE;
			}

			client->nick = g_strdup(l->args[1]); /* Save nick */
		} else if(!g_strcasecmp(l->args[0], "USER")) {

			if (l->argc < 5) {
				client_send_args(client, "461", client->nick?client->nick:"*", 
								 l->args[0], "Not enough parameters", NULL);
				free_line(l);
				return TRUE;
			}
			
			g_free(client->username);
			client->username = g_strdup(l->args[1]);

			g_free(client->hostname);
			client->hostname = g_strdup(l->args[2]);

			g_free(client->fullname);
			client->fullname = g_strdup(l->args[4]);

		} else if(!g_strcasecmp(l->args[0], "PASS")) {
			/* Silently drop... */
		} else if(!g_strcasecmp(l->args[0], "CONNECT")) {
			if (l->argc < 3) {
				client_send_args(client, "461", client->nick?client->nick:"*",
								 l->args[0], "Not enough parameters", NULL);
				free_line(l);
				return TRUE;
			}

			client->network = find_network_by_hostname(l->args[1], atoi(l->args[2]), TRUE);

            if (client->network && 
				client->network->state == NETWORK_STATE_NOT_CONNECTED && 
				!connect_network(client->network)) {
				log_network(NULL, client->network, "Unable to connect");
			}
		} else {
			client_send_args(client, "451", "*", "Register first", client->network?client->network->name:get_my_hostname(), NULL);
		}

		free_line(l);

		if (client->fullname && client->nick) {
			if (!client->network) {
				disconnect_client(client, "Please select a network first, or specify one in your ctrlproxyrc");
				return FALSE;
			}

			welcome_client(client);

			client->incoming_id = g_io_add_watch(client->incoming, G_IO_IN | G_IO_HUP, handle_client_receive, client);

			client->network->clients = g_list_append(client->network->clients, client);
			log_client(NULL, client, "New client");

			return FALSE;
		}

		return TRUE;
	}

	return TRUE;

}

static gboolean client_ping(gpointer user_data) {
	struct client *client = user_data;

	client->last_ping = time(NULL);
	client_send_args(client, "PING", client->network->name, NULL);

	return TRUE;
}


struct client *new_client(struct network *n, GIOChannel *c, const char *desc)
{
	struct client *client = g_new0(struct client, 1);

	g_io_channel_set_close_on_unref(c, TRUE);
	g_io_channel_set_encoding(c, NULL, NULL);
	g_io_channel_set_flags(c, G_IO_FLAG_NONBLOCK, NULL);
	client->connect_time = time(NULL);
	client->ping_id = g_timeout_add(1000 * 300, client_ping, client);
	client->incoming = c;
	client->network = n;
	client->description = g_strdup(desc);
	client->exit_on_close = FALSE;

	if (!desc) {
		socklen_t len = sizeof(struct sockaddr_in6);
		struct sockaddr *sa = g_malloc(len);
		char hostname[NI_MAXHOST];
		char service[NI_MAXSERV];
		int fd = g_io_channel_unix_get_fd(c);

		if (getpeername (fd, sa, &len) == 0 &&
			getnameinfo(sa, len, hostname, sizeof(hostname),
						service, sizeof(service), NI_NOFQDN | NI_NUMERICSERV) == 0) {

			client->description = g_strdup_printf("%s:%s", hostname, service);
		}
	}

	client->incoming_id = g_io_add_watch(client->incoming, G_IO_IN | G_IO_HUP, handle_pending_client_receive, client);

	pending_clients = g_list_append(pending_clients, client);
	return client;
}

void kill_pending_clients(const char *reason)
{
	while(pending_clients) disconnect_client(pending_clients->data, reason);
}
