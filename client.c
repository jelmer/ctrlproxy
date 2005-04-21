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

static gboolean process_from_client(struct line *l)
{
	struct line *lc;
	
	l->direction = TO_SERVER;
	l->origin = g_strdup(l->client->network->hostmask);

	if (!run_client_filter(l)) 
		return TRUE;

	if(!g_strcasecmp(l->args[0], "QUIT")) {
		disconnect_client(l->client);
		return FALSE;
	} else	if(!g_strcasecmp(l->args[0], "PING")) {
		irc_sendf(l->client->incoming, ":%s PONG :%s\r\n", l->client->network->name, l->args[1]);
	} else if(network_is_connected(l->client->network)) {
		char *old_origin;

		state_handle_data(l->client->network, l);

		if (!run_server_filter(l))
			return TRUE;

		run_log_filter(lc = linedup(l)); free_line(lc);
		run_replication_filter(lc = linedup(l)); free_line(lc);

		if (!l->client) {
			return FALSE;
		}

		if(!(l->options & LINE_DONT_SEND)) {
			network_send_line(l->client->network, l);
		}

		/* Also write this message to all other clients currently connected */
		if(!(l->options & LINE_IS_PRIVATE) && l->args[0] &&
		   (!strcmp(l->args[0], "PRIVMSG") || !strcmp(l->args[0], "NOTICE"))) {
			old_origin = l->origin; l->origin = l->client->network->nick;
			clients_send(l->client->network, l, l->client);
			l->origin = old_origin;
		}
	} else {
		irc_sendf(l->client->incoming, ":%s NOTICE %s :Currently not connected to server, connecting...", (l->client->network->name?l->client->network->name:"ctrlproxy"), l->client->network->nick);
		connect_network(l->client->network);
	}

	return TRUE;
}

gboolean client_send_line(struct client *c, const struct line *l)
{
	/* FIXME: Filter */
	return irc_send_line(c->incoming, l);
}

void disconnect_client(struct client *c) {
	if(!c->incoming)return;

	g_source_remove(c->incoming_id);
	c->incoming = NULL;

	if (c->network) {
		c->network->clients = g_list_remove(c->network->clients, c);
	} 

	pending_clients = g_list_remove(pending_clients, c);

	lose_client_hook_execute(c);

	log_client(NULL, c, "Removed client");

	g_free(c->username);
	g_free(c->fullname);
	g_free(c->nick);
	g_free(c);
}

void send_motd(struct network *n, GIOChannel *c)
{
	char **lines;
	int i;
	lines = get_motd_lines(n);

	if(!lines) {
		irc_sendf(c, ":%s %d %s :No MOTD file\r\n", n->name, ERR_NOMOTD, n->nick);
		return;
	}

	irc_sendf(c, ":%s %d %s :Start of MOTD\r\n", n->name, RPL_MOTDSTART, n->nick);
	for(i = 0; lines[i]; i++) {
		irc_sendf(c, ":%s %d %s :%s\r\n", n->name, RPL_MOTD, n->nick, lines[i]);
		g_free(lines[i]);
	}
	g_free(lines);
	irc_sendf(c, ":%s %d %s :End of MOTD\r\n", n->name, RPL_ENDOFMOTD, n->nick);
}

static gboolean handle_client_receive(GIOChannel *c, GIOCondition cond, void *_client)
{
	gboolean ret;
	struct client *client = (struct client *)_client;
	struct line *l;

	if (cond & G_IO_HUP) {
		disconnect_client(client);
		return FALSE;
	}

	if (cond & G_IO_IN) {
		GError *error= NULL;
		GIOStatus status = irc_recv_line(c, &error, &l);

		if (status != G_IO_STATUS_NORMAL) {
			disconnect_client(client);
			return FALSE;
		}

		if(!l) return TRUE;

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
	irc_sendf(client->incoming, ":%s 001 %s :Welcome to the ctrlproxy\r\n", client->network->name, client->network->nick);
	irc_sendf(client->incoming, ":%s 002 %s :Host %s is running ctrlproxy\r\n", client->network->name, client->network->nick, get_my_hostname());
	irc_sendf(client->incoming, ":%s 003 %s :Ctrlproxy (c) 2002-2005 Jelmer Vernooij <jelmer@vernstok.nl>\r\n", client->network->name, client->network->nick);
	irc_sendf(client->incoming, ":%s 004 %s %s %s %s %s\r\n", 
	  client->network->name, client->network->nick, client->network->name, ctrlproxy_version(), client->network->supported_modes[0]?client->network->supported_modes[0]:ALLMODES, client->network->supported_modes[1]?client->network->supported_modes[1]:ALLMODES);

	features = network_generate_feature_string(client->network);

	irc_sendf(client->incoming, ":%s 005 %s %s :are supported on this server\r\n", client->network->name, client->network->nick, features);

	g_free(features);

	send_motd(client->network, client->incoming);

	if(!new_client_hook_execute(client)) {
		disconnect_client(client);
		return FALSE;
	} else {
		/* Initialization was successful. Now we can almost safely try to change to the nick, which
		 * was requested by the client earlier */
		if (!client->network->ignore_first_nick && client->nick)
			if (strcmp(client->nick, client->network->nick)) 
				network_send_args(client->network, "NICK", client->nick, NULL);
	}

	return TRUE;
}

static gboolean handle_pending_client_receive(GIOChannel *c, GIOCondition cond, void *_client)
{
	struct client *client = (struct client *)_client;
	struct line *l;

	if (cond & G_IO_HUP) {
		disconnect_client(client);
		return FALSE;
	}

	if (cond & G_IO_IN) {
		GError *error = NULL;
		GIOStatus status = irc_recv_line(c, &error, &l);
		
		if (status != G_IO_STATUS_NORMAL) {
			disconnect_client(client);
			return FALSE;
		}

		if(!l) return TRUE;

		/* Silently drop empty messages */
		if (l->argc == 0) {
			free_line(l);
			return TRUE;
		}

		if(!g_strcasecmp(l->args[0], "NICK") && l->args[1] && !client->nick) {
			/* Don't allow the client to change the nick now. We would change it after initialization */
			if (strcmp(l->args[1], client->network->nick)) 
				irc_sendf(client->incoming, ":%s NICK %s", l->args[1], client->network->nick);

			client->nick = g_strdup(l->args[1]); /* Save nick */

			welcome_client(client);

			client->incoming_id = g_io_add_watch(client->incoming, G_IO_IN | G_IO_HUP, handle_client_receive, client);

			client->network->clients = g_list_append(client->network->clients, client);
			log_client(NULL, client, "New client");
			free_line(l);

			return FALSE;
		} else if(!g_strcasecmp(l->args[0], "USER")) {
			if (l->argc > 1) {
				g_free(client->username);
				client->username = g_strdup(l->args[1]);
			}

			if (l->argc > 4) {
				g_free(client->fullname);
				client->fullname = g_strdup(l->args[4]);
			}

			if (!client->network) {
				irc_sendf(client->incoming, "ERROR :Please select a network first, or specify one in your ctrlproxyrc\r\n");
				disconnect_client(client);
				return FALSE;
			}
		} else if(!g_strcasecmp(l->args[0], "CONNECT")) {
			client->network = find_network_by_hostname(l->args[1], atoi(l->args[2]), TRUE);

            if (client->network && !network_is_connected(client->network) && !connect_network(client->network)) {
				log_network(NULL, client->network, "Unable to connect");
			}
		} else {
			irc_sendf(client->incoming, ":%s 451 * :Register first\r\n"	, client->network?client->network->name:get_my_hostname());
		}

		free_line(l);

		return TRUE;
	}

	return TRUE;

}

struct client *new_client(struct network *n, GIOChannel *c, const char *desc)
{
	struct client *client = g_new0(struct client, 1);

	g_io_channel_set_encoding(c, NULL, NULL);
	g_io_channel_set_flags(c, G_IO_FLAG_NONBLOCK, NULL);
	client->connect_time = time(NULL);
	client->incoming = c;
	client->network = n;
	client->description = g_strdup(desc);

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

void kill_pending_clients()
{
	while(pending_clients) disconnect_client(pending_clients->data);
}
