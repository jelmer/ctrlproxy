/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2003 Jelmer Vernooij <jelmer@nl.linux.org>

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define SERVER_SEND_LINE(s,l) {if(!irc_send_line((s)->outgoing, l))reconnect(NULL, s);}
#define CLIENT_SEND_LINE(c,l) {if(!irc_send_line((c)->incoming, l))disconnect_client(c);}

GList *networks = NULL;

extern FILE *debugfd;

extern char *debugfile;

static void reconnect(struct transport_context *c, void *_server);

void handle_server_receive (struct transport_context *c, char *raw, void *_server)
{
	struct network *server = (struct network *)_server;
	struct line *l;
	
	if(debugfd)fprintf(debugfd, _("[From server] %s\n"), raw);

	l = irc_parse_line(raw);
	if(!l)return;

	/* Silently drop empty messages, as allowed by RFC */
	if(l->argc == 0) {
		free_line(l);
		return;
	}
	
	l->direction = FROM_SERVER;
	l->network = server;

	filters_execute(l);
	state_handle_data(server,l);

	/* We need to handle pings.. we can't depend on a client
	 * to do that for us*/
	l->direction = FROM_SERVER;
	if(l->args[0]) {
		if(!strcasecmp(l->args[0], "PING")){
			if(server->outgoing)irc_send_args(server->outgoing, "PONG", l->args[1], NULL);
		} else if(!strcasecmp(l->args[0], "PONG")){
		} else if(!strcasecmp(l->args[0], "433") && !server->authenticated){
			if(server->outgoing) {
				char *new_nick;
				char *nick = xmlGetProp(server->xmlConf, "nick");
				asprintf(&new_nick, "%s_", nick);
				xmlSetProp(server->xmlConf, "nick", new_nick);
				irc_send_args(server->outgoing, "NICK", new_nick, NULL);
				xmlFree(nick);
				free(new_nick);
			}
		} else if(server->authenticated) {
			if(!(l->options & LINE_DONT_SEND))clients_send(server, l, NULL);
		} else if(atoi(l->args[0]) == 4) {
			xmlNodePtr cur = server->xmlConf->xmlChildrenNode;
			server_connected_hook_execute(server);
			server->authenticated = 1;
			while(cur) {
				if(!strcmp(cur->name, "autosend")) {
					xmlChar *data = xmlNodeGetContent(cur);
					struct line *newl;
					
					newl = irc_parse_line(data);

					SERVER_SEND_LINE(server, newl);

					xmlFree(data);
					free_line(newl);
				}
				cur = cur->next;
			}
			
			/* Rejoin channels */
			cur = server->xmlConf->xmlChildrenNode;
			while(cur) {
				if(!strcmp(cur->name, "channel")) {
					char *n = xmlGetProp(cur, "name");
					char *a = xmlGetProp(cur, "autojoin");
					char *k=  xmlGetProp(cur, "key");
					g_assert(n);
					if(a && atoi(a))
						irc_send_args(server->outgoing, "JOIN", n, k, NULL);
					if(k)xmlFree(k);
					if(n)xmlFree(n);
					if(a)xmlFree(a);
				} 
				cur = cur->next;
			}
		}
	}
	free_line(l); l = NULL;
}

xmlNodePtr network_get_next_server(struct network *n)
{
	xmlNodePtr cur = n->current_server;
	/* Get next available server */
	if(cur) cur = cur->next;

	while(cur && (xmlIsBlankNode(cur) || !strcmp(cur->name, "comment"))) cur = cur->next;

	if(cur) return cur;

	cur = n->servers;

	while(cur && (xmlIsBlankNode(cur) || !strcmp(cur->name, "comment"))) cur = cur->next;

	return cur;
}

gboolean connect_next_server(struct network *s) 
{
	s->current_server = network_get_next_server(s);
	return connect_current_server(s);
}

void server_send_login (struct transport_context *c, void *_server) {
	struct network *s = (struct network *)_server;
	char *server_name = xmlGetProp(s->xmlConf, "name");
	char *nick, *username, *fullname, *password;

	g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, _("Successfully connected to %s"), server_name);

	nick = xmlGetProp(s->xmlConf, "nick");
	username = xmlGetProp(s->xmlConf, "username");
	fullname = xmlGetProp(s->xmlConf, "fullname");
	password = xmlGetProp(s->xmlConf, "password");
	g_assert(strlen(nick));
	g_assert(strlen(username));
	g_assert(strlen(fullname));
	g_assert(strlen(my_hostname));
	g_assert(strlen(server_name));

	if(xmlHasProp(s->xmlConf, "password"))irc_send_args(s->outgoing, "PASS", password, NULL);
	irc_send_args(s->outgoing, "NICK", nick, NULL);
	irc_send_args(s->outgoing, "USER", username, my_hostname, server_name, fullname, NULL);

	xmlFree(nick);
	xmlFree(username);
	xmlFree(fullname);
	xmlFree(password);

}

gboolean connect_current_server(struct network *s) {
	char *server_name = xmlGetProp(s->xmlConf, "name");

	if(!s->current_server){
		xmlSetProp(s->xmlConf, "autoconnect", "0");
		g_warning(_("No servers listed for network %s, not connecting\n"), server_name);
		xmlFree(server_name);
		return TRUE;
	}

	g_message(_("Connecting with %s for server %s"), s->current_server->name, server_name);

	s->outgoing = transport_connect(s->current_server->name, s->current_server, handle_server_receive, server_send_login, reconnect, s);

	if(!xmlHasProp(s->xmlConf, "name") && xmlHasProp(s->current_server, "name")) {
		xmlFree(server_name);
		s->name_guessed = TRUE;
		server_name = xmlGetProp(s->current_server, "name");
		xmlSetProp(s->xmlConf, "name", server_name);
	}

	if(!s->outgoing) {
		g_warning(_("Couldn't connect with network %s via transport %s"), server_name, s->current_server->name);
		xmlFree(server_name);
		return TRUE;
	}

	xmlFree(server_name);

	return FALSE;
}

void reconnect(struct transport_context *c, void *_server)
{
	struct network *server = (struct network *)_server;
	char *server_name;

	/* Don't report disconnections twice */
	g_assert(server);
	server_name = xmlGetProp(server->xmlConf, "name");

	if(!server->outgoing) return;
	server_disconnected_hook_execute(server);
	transport_free(server->outgoing); server->outgoing = NULL;

	g_warning(_("Connection to server %s lost, trying to reconnect..."), server_name);
	xmlFree(server_name);

	server->authenticated = 0;
	state_reconnect(server);
	server->reconnect_id = g_timeout_add(RECONNECT_INTERVAL, (GSourceFunc) connect_next_server, server);
}

gboolean close_server(struct network *n) {
	int i;

	if(n->reconnect_id) {
		g_source_remove(n->reconnect_id);
		n->reconnect_id = 0;
	}

	if(n->outgoing) {
		irc_send_args(n->outgoing, "QUIT", NULL);
		server_disconnected_hook_execute(n);
		transport_free(n->outgoing);
		n->outgoing = NULL;
		free(n->hostmask);

		for(i = 0; i < 2; i++) {
			if(n->supported_modes[i]) {
				free(n->supported_modes[i]);
				n->supported_modes[i] = NULL;
			}
		}

		if(n->features){
			for(i = 0; n->features[i]; i++)
				free(n->features[i]);
			free(n->features);
			n->features = NULL;
		}

		n->authenticated = 0;

		return TRUE;
	}
	return FALSE;
}

void disconnect_client(struct client *c) {
	char *networkname;
	if(!c->incoming)return;

	transport_free(c->incoming);
	c->incoming = NULL;

	c->network->clients = g_list_remove(c->network->clients, c);
	if(c->authenticated) lose_client_hook_execute(c);
	c->authenticated = 2;

	networkname = xmlGetProp(c->network->xmlConf, "name");
	g_message(_("Removed client to %s"), networkname);
	g_free(networkname);
	
	free(c);

}

void clients_send(struct network *server, struct line *l, struct transport_context *exception) 
{
	GList *g = server->clients;
	while(g) {
		struct client *c = (struct client *)g->data;
		if(c->authenticated && c->incoming != exception)CLIENT_SEND_LINE(c, l);
		g = g_list_next(g);
	}
}

void handle_client_disconnect(struct transport_context *c, void *data)
{
	struct client *client = (struct client *)data;
	disconnect_client(client);
}

void send_motd(struct network *n, struct transport_context *c)
{
	char **lines;
	int i;
	char *server_name = xmlGetProp(n->xmlConf, "name");
	char *nick = xmlGetProp(n->xmlConf, "nick");

	lines = get_motd_lines(n);

	if(!lines) {
		irc_sendf(c, ":%s %d %s :No MOTD file\r\n", server_name, ERR_NOMOTD, nick);
		xmlFree(nick); xmlFree(server_name);
		return;
	}

	irc_sendf(c, ":%s %d %s :Start of MOTD\r\n", server_name, RPL_MOTDSTART, nick);
	for(i = 0; lines[i]; i++) {
		irc_sendf(c, ":%s %d %s :%s\r\n", server_name, RPL_MOTD, nick, lines[i]);
		free(lines[i]);
	}
	free(lines);
	irc_sendf(c, ":%s %d %s :End of MOTD\r\n", server_name, RPL_ENDOFMOTD, nick);
	xmlFree(nick); xmlFree(server_name);
}

void handle_client_receive(struct transport_context *c, char *raw, void *_client)
{
	struct client *client = (struct client *)_client;
	char allmodes[] = "aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ";
	struct line *l;
	char *clientpass;
	char *server_name, *nick;
	char *tmp;

	if(client->authenticated == 2)return;
	
	if(debugfd)fprintf(debugfd, _("[From client] %s\n"), raw);

	l = irc_parse_line(raw);
	if(!l)return;

	/* Silently drop empty messages */
	if(l->argc == 0) {
		free_line(l);
		return;
	}
	
	l->direction = TO_SERVER;
	l->client = client;
	l->network = client->network;

	clientpass = xmlGetProp(client->network->xmlConf, "client_pass");
	if(!l->args[0]){ 
		free_line(l);
		return;
	}

	server_name = xmlGetProp(client->network->xmlConf, "name");
	nick = xmlGetProp(client->network->xmlConf, "nick");

	if(!clientpass && !client->authenticated) {
		client->authenticated = 1;
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, _("No password set for %s, allowing client _without_ authentication!"), server_name);
	}

	if(!strcasecmp(l->args[0], "USER")){
		if(!client->authenticated){
			irc_sendf(c, ":%s 451 %s :You are not registered\r\n", 
					  server_name,
					  nick);
			free_line(l);l = NULL;
			if(clientpass)xmlFree(clientpass);
			xmlFree(server_name); xmlFree(nick);
			return;
		}
		irc_sendf(c, ":%s 001 %s :Welcome to the ctrlproxy\r\n", server_name, nick);
		irc_sendf(c, ":%s 002 %s :Host %s is running ctrlproxy\r\n", server_name, nick, my_hostname);
		irc_sendf(c, ":%s 003 %s :Ctrlproxy (c) 2002 Jelmer Vernooij <jelmer@nl.linux.org>\r\n", server_name, nick);
		irc_sendf(c, ":%s 004 %s %s %s %s %s\r\n", 
				server_name, nick, server_name, PACKAGE_VERSION, client->network->supported_modes[0]?client->network->supported_modes[0]:allmodes, client->network->supported_modes[1]?client->network->supported_modes[1]:allmodes);
		
		if(client->network->features) {
			tmp = list_make_string(client->network->features);
		
			irc_sendf(c, ":%s 005 %s %s :are supported on this server\r\n", server_name, nick, tmp);
			free(tmp);
		}

		send_motd(client->network, c);
		g_message(_("Client @%s successfully authenticated"),
			  server_name);
		if(!new_client_hook_execute(client))
			disconnect_client(client);
	} else if(!strcasecmp(l->args[0], "PASS")) {
		if (!clientpass)
			client->authenticated = 1;
		else if(!strcmp(l->args[1], clientpass)) {
			client->authenticated = 1;
		} else {
			g_warning(_("User tried to log in to %s with incorrect password!\n"), server_name);
			irc_sendf(c, ":%s %d %s :Password mismatch\r\n", server_name, ERR_PASSWDMISMATCH, nick);
			disconnect_client(client);
			free_line(l);
			if(clientpass) xmlFree(clientpass);
			xmlFree(nick); xmlFree(server_name);
			return;
		}
	} else if(!strcasecmp(l->args[0], "NICK") && l->args[1] && !client->did_nick_change && xmlHasProp(client->network->xmlConf, "ignore_first_nick")) {
		/* Ignore the first nick change attempt */
		client->did_nick_change = 1;
		irc_sendf(c, ":%s NICK %s", l->args[1], nick);
	} else if(!strcasecmp(l->args[0], "NICK") && l->args[1] && !strcasecmp(l->args[1], nick)) {
		/* Ignore attempts to change nick to the current nick */
	} else if(!strcasecmp(l->args[0], "QUIT")) {
		disconnect_client(client);
		free_line(l);
		if(clientpass)xmlFree(clientpass);
		return;
	} else if(!strcasecmp(l->args[0], "PING")) {
		irc_sendf(c, ":%s PONG :%s\r\n", server_name, l->args[1]);
	} else if(client->authenticated) {
		state_handle_data(client->network, l);
	
		filters_execute(l);

		client = l->client;
		if (!client) return;

		if(client->network->outgoing) {
			const char *old_origin;
			if(!(l->options & LINE_DONT_SEND)) {SERVER_SEND_LINE(client->network, l) }

			/* Also write this message to all other clients currently connected */
			if(!(l->options & LINE_IS_PRIVATE) && l->args[0] &&
			   (!strcmp(l->args[0], "PRIVMSG") || !strcmp(l->args[0], "NOTICE"))) {
				old_origin = l->origin; l->origin = nick;
				clients_send(client->network, l, c);
				l->origin = old_origin;
			}
		} else {
			irc_sendf(c, ":%s NOTICE %s :Currently not connected to server, ignoring\r\n", (server_name != NULL?server_name:"ctrlproxy"), nick);
		}
	} else {
		irc_sendf(c, ":%s NOTICE %s :You are not logged in yet. Ignoring\r\n", server_name?server_name:"ctrlproxy", nick);
	}
	free_line(l);l = NULL;
	if(clientpass)xmlFree(clientpass);
	xmlFree(server_name); xmlFree(nick);
}

void handle_new_client(struct transport_context *c_server, struct transport_context *c_client, void *_server)
{
	struct network *n = (struct network *)_server;
	struct client *d = malloc(sizeof(struct client));

	d->authenticated = 0;
	d->network = n;
	d->connect_time = time(NULL);
	d->incoming = c_client;
	d->did_nick_change = 0;

	n->clients = g_list_append(n->clients, d);

	transport_set_receive_handler(c_client, handle_client_receive);
	transport_set_disconnect_handler(c_client, handle_client_disconnect);
	transport_set_data(c_client, d);
}

void network_add_listen(struct network *n, xmlNodePtr listen)
{
	int i = 0;
	struct transport_context *t = transport_listen(listen->name, listen, handle_new_client, n);

	if(n->incoming)	{ for(i = 0; n->incoming[i]; i++); }
	
	if(!t) {
		g_warning(_("Can't initialise transport %s"), listen->name);
		return;
	} 
	
	n->incoming = realloc(n->incoming, (i+2) * sizeof(struct transport_context *));
	n->incoming[i] = t;
	n->incoming[i+1] = NULL;

}

struct network *connect_network(xmlNodePtr conf) {
	struct network *s = malloc(sizeof(struct network));
	char *nick, *user_name;
	xmlNodePtr cur;

	memset(s, 0, sizeof(struct network));
	s->xmlConf = conf;
	if(!xmlHasProp(s->xmlConf, "nick"))
		xmlSetProp(s->xmlConf, "nick", g_get_user_name());

	if(!xmlHasProp(s->xmlConf, "username"))
		xmlSetProp(s->xmlConf, "username", g_get_user_name());

	if(!xmlHasProp(s->xmlConf, "fullname"))
		xmlSetProp(s->xmlConf, "fullname", g_get_real_name());

   	nick = xmlGetProp(s->xmlConf, "nick");
	user_name = xmlGetProp(s->xmlConf, "username");
	asprintf(&s->hostmask, "%s!~%s@%s", nick, user_name, my_hostname);
	xmlFree(nick); xmlFree(user_name);

	/* Find <listen> tag */
	s->listen = NULL;
	s->incoming = malloc(sizeof(struct transport_context *));
	s->incoming[0] = NULL;
	s->listen = xmlFindChildByElementName(s->xmlConf, "listen");

	if(s->listen)cur = s->listen->xmlChildrenNode; else cur = NULL;

	while(cur) {
		if(xmlIsBlankNode(cur) || !strcmp(cur->name, "comment")) {
			cur = cur->next;
			continue;
		}

		network_add_listen(s, cur);

		cur = cur->next;
	}

	s->servers = NULL;
	s->current_server = NULL;

	/* Find <servers> tag */
	s->servers = xmlFindChildByElementName(s->xmlConf, "servers");
	s->servers = s->servers->xmlChildrenNode;

	if(!s->servers) {
		char *server_name = xmlGetProp(s->xmlConf, "name");
		g_warning(_("No servers listed for network %s!\n"), server_name);
		xmlFree(server_name);
	}

	/* Add server by default. If connecting succeeds, it is
	 * removed automagically by connect_current_server */
	connect_next_server(s);
	networks = g_list_append(networks, s);

	return s;
}

int close_network(struct network *s)
{
	GList *l = s->clients;
	int i;
	char *server_name = xmlGetProp(s->xmlConf, "name");
	g_assert(s);
	g_message(_("Closing connection to %s"), server_name);

	while(l) {
		struct client *c = l->data;
		irc_sendf(c->incoming, ":%s QUIT :Server exiting\r\n", server_name);
		l = l->next;
		disconnect_client(c);
	}
	g_list_free(s->clients);s->clients = NULL;

	/* Remove all listening lines */
	if(s->incoming) {
		for(i = 0; s->incoming[i] != NULL; i++) transport_free(s->incoming[i]);
		free(s->incoming);
	}

	close_server(s);

	networks = g_list_remove(networks, s);

	if(s->reconnect_id) g_source_remove(s->reconnect_id);

	free(s);

	xmlFree(server_name);
	if(!networks)clean_exit();
	return 0;
}


gboolean ping_loop(gpointer user_data) {
	GList *l = networks;
	while(l) {
		struct network *s = (struct network *)l->data;
		GList *cl;
		char *server_name = xmlGetProp(s->xmlConf, "name");
		if(s->outgoing)irc_send_args(s->outgoing, "PING", server_name, NULL);
		else reconnect(NULL, s);
		xmlFree(server_name);

		/* Throw out unauthorized clients that have been inactive for over a minute */
		cl = s->clients;
		while(cl) {
			struct client *c = (struct client *)cl->data;
			cl = g_list_next(cl);
			if(c->authenticated)continue;
			if(time(NULL) - c->connect_time > 60) {
				disconnect_client(c);
			}
		}
		
		l = g_list_next(l);
	}
	return TRUE;
}

int verify_client(struct network *s, struct client *c)
{
	GList *gl = s->clients;
	while(gl) {
		struct client *nc = (struct client *)gl->data;
		if(c == nc)return 1;
		gl = gl->next;
	}
	
	return 0;
}
