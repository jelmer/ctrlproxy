/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2004 Jelmer Vernooij <jelmer@nl.linux.org>

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

#define __USE_POSIX
#include <netinet/in.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "internals.h"
#include "irc.h"

static GIOChannel * (*sslize_function) (GIOChannel *);

static GList *networks = NULL;

extern FILE *debugfd;

extern char *debugfile;

static GHashTable *virtual_network_ops = NULL;

static void reconnect(struct network *server, gboolean rm_source);

static void server_send_login (struct network *s) 
{
	log_network(NULL, s, "Successfully connected");

	if(s->type == NETWORK_TCP && s->connection.tcp.current_server->password) { 
		network_send_args(s, "PASS", s->connection.tcp.current_server->password, NULL);
	} else if (s->password) {
		network_send_args(s, "PASS", s->password, NULL);
	}
	network_send_args(s, "NICK", s->nick, NULL);
	network_send_args(s, "USER", s->username, get_my_hostname(), s->name, s->fullname, NULL);

	s->login_sent = TRUE;
}

static gboolean process_from_server(struct line *l)
{
	struct line *lc;

	l->direction = FROM_SERVER;

	run_log_filter(lc = linedup(l)); free_line(lc);
	run_replication_filter(lc = linedup(l)); free_line(lc);

	state_handle_data(l->network,l);

	/* We need to handle pings.. we can't depend on a client
	 * to do that for us*/
	if(!g_strcasecmp(l->args[0], "PING")){
		network_send_args(l->network, "PONG", l->args[1], NULL);
	} else if(!g_strcasecmp(l->args[0], "PONG")){
	} else if(!g_strcasecmp(l->args[0], "ERROR")) {
		log_network(NULL, l->network, "error: %s", l->args[1]);
	} else if(!g_strcasecmp(l->args[0], "433") && !l->network->authenticated){
		char *old_nick = l->network->nick;
		l->network->nick = g_strdup_printf("%s_", l->network->nick);
		network_send_args(l->network, "NICK", l->network->nick, NULL);
		g_free(old_nick);
	} else if(atoi(l->args[0]) == 4) {
		GList *gl;

		server_connected_hook_execute(l->network);
		l->network->authenticated = TRUE;

		for (gl = l->network->autosend_lines; gl; gl = gl->next) {
			char *data = gl->data;
			struct line *newl;

			newl = irc_parse_line(data);

			network_send_line(l->network, newl);

			free_line(newl);
		}

		/* Rejoin channels */
		for (gl = l->network->channels; gl; gl = gl->next) 
		{
			struct channel *c = gl->data;
			if(c->autojoin) {
				network_send_args(l->network, "JOIN", c->name, c->key, NULL);
			} 
		}

		/* Make sure the current server is listed as a possible one for this network */
		if (l->network->type == NETWORK_TCP && l->argc > 2) {
			for (gl = l->network->connection.tcp.servers; gl; gl = gl->next) {
				struct tcp_server *tcp = gl->data;

				if (!g_strcasecmp(tcp->host, l->args[1])) 
					break;
			}

			/* Not found, add new one */
			if (!gl) {
				struct tcp_server *tcp = g_new0(struct tcp_server, 1);
				int error;
				struct addrinfo hints;

				tcp->host = g_strdup(l->args[2]);
				tcp->name = g_strdup(l->args[2]);
				tcp->port = g_strdup(l->network->connection.tcp.current_server->port);
				tcp->ssl = l->network->connection.tcp.current_server->ssl;
				tcp->password = g_strdup(l->network->connection.tcp.current_server->password);

				memset(&hints, 0, sizeof(hints));
				hints.ai_family = PF_UNSPEC;
				hints.ai_socktype = SOCK_STREAM;

				error = getaddrinfo(tcp->host, tcp->port, &hints, &tcp->addrinfo);
				if (error) {
					log_network(NULL, l->network, "Unable to lookup %s: %s", tcp->host, gai_strerror(error));
				} else {
					l->network->connection.tcp.servers = g_list_append(l->network->connection.tcp.servers, tcp);
				}
			}

		}
	} 

	if(!(l->options & LINE_DONT_SEND)) {
		if( run_server_filter(l)) {
			clients_send(l->network, l, NULL);
		} 
	} 

	return TRUE;
}

gboolean handle_server_receive (GIOChannel *c, GIOCondition cond, void *_server)
{
	struct network *server = (struct network *)_server;
	struct line *l;
	gboolean ret;

	if (cond & G_IO_HUP || cond & G_IO_ERR) {
		reconnect(server, FALSE);
		return FALSE;
	}

	if (!server->authenticated && !server->login_sent) {
		server_send_login(server);
	}

	if (cond & G_IO_IN) {
		GError *err = NULL;
		GIOStatus status = irc_recv_line(c, &err, &l);

		if (status != G_IO_STATUS_NORMAL) {
			reconnect(server, FALSE);
			return FALSE;
		}
		
		if(!l) return TRUE;

		/* Silently drop empty messages, as allowed by RFC */
		if(l->argc == 0) {
			free_line(l);
			return TRUE;
		}

		l->network = server;

		ret = process_from_server(l);

		free_line(l);

		return ret;
	}

	return TRUE;
}

struct tcp_server *network_get_next_tcp_server(struct network *n)
{
	GList *cur = g_list_find(n->connection.tcp.servers, n->connection.tcp.current_server);

	/* Get next available server */
	if(cur && cur->next) cur = cur->next; else cur = n->connection.tcp.servers;

	if(cur) return cur->data; 

	return NULL;
}

gboolean connect_next_tcp_server(struct network *s) 
{
	s->connection.tcp.current_server = network_get_next_tcp_server(s);
	return connect_current_tcp_server(s);
}

gboolean network_send_line(struct network *s, const struct line *ol)
{
	struct line l = *ol;
	
	l.origin = NULL;		/* Never send origin to the server */
	l.network = s;

	switch (s->type) {
	case NETWORK_TCP:
		return irc_send_line(s->connection.tcp.outgoing, &l);

	case NETWORK_PROGRAM:
		return irc_send_line(s->connection.program.outgoing, &l);

	case NETWORK_VIRTUAL:
		if (!s->connection.virtual.ops) 
			return FALSE;
		return s->connection.virtual.ops->to_server(s, &l);

	default:
		return FALSE;
	}
}

gboolean virtual_network_recv_line(struct network *s, struct line *l)
{
	l->network = s;

	if (!l->origin) 
		l->origin = g_strdup(s->name);

	return process_from_server(l);
}

gboolean virtual_network_recv_args(struct network *s, const char *origin, ...)
{
	va_list ap;
	struct line *l;
	gboolean ret;

	va_start(ap, origin);
	l = virc_parse_line(origin, ap);
	va_end(ap);

	ret = virtual_network_recv_line(s, l);

	free_line(l);

	return ret;
}

gboolean network_send_args(struct network *s, ...)
{
	va_list ap;
	struct line *l;
	gboolean ret;
	va_start(ap, s);
	l = virc_parse_line(NULL, ap);
	va_end(ap);

	ret = network_send_line(s, l);

	free_line(l);

	return ret;
}


gboolean connect_current_tcp_server(struct network *s) 
{
	struct addrinfo *res;
	int sock;
	size_t size;
	struct tcp_server *cs = s->connection.tcp.current_server;
	GIOChannel *ioc = NULL;

	if(!cs) {
		s->autoconnect = FALSE;
		log_network(NULL, s, "No servers listed, not connecting");
		return TRUE;
	}

	log_network(NULL, s, "Connecting with %s:%s", 
			  cs->host, 
			  cs->port);

	/* Connect */

	for (res = cs->addrinfo; res; res = res->ai_next) {
		sock = socket(res->ai_family, res->ai_socktype,
					  res->ai_protocol);
		if (sock < 0) {
			continue;
		}

		ioc = g_io_channel_unix_new(sock);

		g_io_channel_set_flags(ioc, G_IO_FLAG_NONBLOCK, NULL);
		g_io_channel_set_encoding(ioc, NULL, NULL);

		if (connect(sock, res->ai_addr, res->ai_addrlen) < 0 && errno != EINPROGRESS) {
			g_io_channel_unref(ioc);
			ioc = NULL;
			continue;
		}

		break; 
	}

	if (!ioc) {
		log_network(NULL, s, "Unable to connect: %s", strerror(errno));
		return FALSE;
	}

	size = sizeof(struct sockaddr_in6);
	g_free(s->connection.tcp.local_name);
	s->connection.tcp.local_name = g_malloc(size);
	s->connection.tcp.namelen = getsockname(sock, s->connection.tcp.local_name, &size);

	s->connection.tcp.outgoing = ioc;

	if (cs->ssl) {
		if (!sslize_function) {
			log_network(NULL, s, "SSL enabled for %s:%s, but no SSL support loaded", cs->host, cs->port);
		} else {
			s->connection.tcp.outgoing = sslize_function(s->connection.tcp.outgoing);
		}
	}

	s->connection.tcp.outgoing_id = g_io_add_watch(s->connection.tcp.outgoing, G_IO_IN | G_IO_HUP | G_IO_ERR, handle_server_receive, s);

	g_io_channel_unref(s->connection.tcp.outgoing);

	if(!s->name && cs->name) {
		s->name_guessed = TRUE;
		s->name = g_strdup(s->name);
	}

	if(!s->connection.tcp.outgoing) {
		log_network(NULL, s, "Couldn't connect via server %s:%s", cs->host, cs->port);
		return TRUE;
	}

	return FALSE;
}

static void reconnect(struct network *server, gboolean rm_source)
{
	/* Don't report disconnections twice */

	server_disconnected_hook_execute(server);

	switch (server->type) {
	case NETWORK_TCP: 
		if (rm_source) g_source_remove(server->connection.tcp.outgoing_id); 
		server->connection.tcp.outgoing = 0; 
		break;
	case NETWORK_PROGRAM: 
		if (rm_source) g_source_remove(server->connection.program.outgoing_id); 
		server->connection.program.outgoing = 0; 
		break;
	default: break;
	}

	server->authenticated = FALSE;
	server->login_sent = FALSE;
	free_channels(server);

	if (server->type == NETWORK_TCP) {
		log_network(NULL, server, "Connection lost, trying to reconnect in %ds...", server->reconnect_interval);
		server->reconnect_id = g_timeout_add(1000 * server->reconnect_interval, (GSourceFunc) connect_next_tcp_server, server);
	} else {
		connect_network(server);	
	}
}

gboolean network_is_connected(struct network *n)
{
	switch (n->type) {
	case NETWORK_TCP: return (n->connection.tcp.outgoing != NULL);
	case NETWORK_PROGRAM: return (n->connection.program.outgoing != NULL);
	case NETWORK_VIRTUAL: return (n->connection.virtual.ops != NULL);
	}

	return FALSE;
}

gboolean close_server(struct network *n) {
	int i;

	if(n->reconnect_id) {
		g_source_remove(n->reconnect_id);
		n->reconnect_id = 0;
	}

	if(network_is_connected(n)) {
		network_send_args(n, "QUIT", NULL);
		server_disconnected_hook_execute(n);
		switch (n->type) {
		case NETWORK_TCP: 
			g_source_remove(n->connection.tcp.outgoing_id); 
			n->connection.tcp.outgoing_id = 0; 
			break;
		case NETWORK_PROGRAM: 
			g_source_remove(n->connection.program.outgoing_id); 
			n->connection.program.outgoing_id = 0; 
			break;
		case NETWORK_VIRTUAL:
			if (n->connection.virtual.ops && n->connection.virtual.ops->fini) {
				n->connection.virtual.ops->fini(n);
			}
			break;
		default: break;
		}

		g_free(n->hostmask);
		n->hostmask = NULL;

		for(i = 0; i < 2; i++) {
			if(n->supported_modes[i]) {
				g_free(n->supported_modes[i]);
				n->supported_modes[i] = NULL;
			}
		}

		g_hash_table_destroy(n->server_features);
		n->server_features = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

		n->authenticated = FALSE;
		n->login_sent = FALSE;

		return TRUE;
	}
	return FALSE;
}

void clients_send(struct network *server, struct line *l, struct client *exception) 
{
	GList *g;
	for (g = server->clients; g; g = g->next) {
		struct client *c = (struct client *)g->data;
		if(c != exception) {
			if(run_client_filter(l)) { 
				client_send_line(c, l);
			}
		}
	}
}

struct network *new_network() 
{
	struct network *s = g_new0(struct network,1);

	s->autoconnect = FALSE;
	s->nick = g_strdup(g_get_user_name());
	s->username = g_strdup(g_get_user_name());
	s->fullname = g_strdup(g_get_real_name());
	s->server_features = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	s->reconnect_interval = DEFAULT_RECONNECT_INTERVAL;

	s->hostmask = g_strdup_printf("%s!~%s@%s", s->nick, s->username, get_my_hostname());

	networks = g_list_append(networks, s);
	return s;
}

static pid_t piped_child(char* const command[], int *f_in)
{
	pid_t pid;
	int sock[2];

	if(socketpair(PF_UNIX, SOCK_STREAM, AF_LOCAL, sock) == -1) {
		log_global(NULL, "socketpair: %s", strerror(errno));
		return -1;
	}

	*f_in = sock[0];

	fcntl(sock[0], F_SETFL, O_NONBLOCK);

	pid = fork();
	if (pid == -1) {
		log_global(NULL, "fork: %s", strerror(errno));
		return -1;
	}

	if (pid == 0) {
		close(0);
		close(1);
		close(2);
		close(sock[0]);

		dup2(sock[1], 0);
		dup2(sock[1], 1);
		execvp(command[0], command);
		log_global(NULL, "Failed to exec %s : %s", command[0], strerror(errno));
		return -1;
	}

	close(sock[1]);

	return pid;
}

static gboolean connect_program(struct network *s)
{
	int sock;
	char *cmd[2] = { s->connection.program.location, NULL };
	pid_t pid = piped_child(cmd, &sock);

	if (pid == -1) return FALSE;

	s->connection.program.outgoing = g_io_channel_unix_new(sock);

	server_send_login(s);
	
	s->connection.program.outgoing_id = g_io_add_watch(s->connection.program.outgoing, G_IO_IN | G_IO_ERR, handle_server_receive, s);

	g_io_channel_unref(s->connection.program.outgoing);

	if(!s->name) {
		s->name_guessed = TRUE;
		if (strchr(s->connection.program.location, '/')) {
			s->name = g_strdup(strrchr(s->connection.program.location, '/')+1);
		} else {
			s->name = g_strdup(s->connection.program.location);
		}
	}

	return TRUE;
}

gboolean connect_network(struct network *s) {

	switch (s->type) {
	case NETWORK_TCP:
		s->connection.tcp.current_server = NULL;
		/* Add server by default. If connecting succeeds, it is
		 * removed automagically by connect_current_tcp_server */
		connect_next_tcp_server(s);
		break;

	case NETWORK_PROGRAM:
		return connect_program(s);

	case NETWORK_VIRTUAL:
		s->connection.virtual.ops = g_hash_table_lookup(virtual_network_ops, s->connection.virtual.type);
		if (!s->connection.virtual.ops) return FALSE;

		if (s->connection.virtual.ops->init) 
			return s->connection.virtual.ops->init(s);

		/* FIXME: Set s->connection.virtual.ops->send */

		return TRUE;
	}

	return TRUE;
}

int close_network(struct network *s)
{
	GList *l = s->clients;
	g_assert(s);
	log_network(NULL, s, "Closing connection");

	while(l) {
		struct client *c = l->data;
		irc_send_args(c->incoming, "ERROR", "Server exiting", NULL);
		l = l->next;
		disconnect_client(c);
	}
	g_list_free(s->clients); s->clients = NULL;

	close_server(s);

	networks = g_list_remove(networks, s);

	if(s->reconnect_id) g_source_remove(s->reconnect_id);

	g_free(s->fullname);
	g_free(s->username);
	g_free(s->nick);
	g_free(s->password);
	g_free(s->name);

	g_hash_table_destroy(s->server_features);

	free_channels(s);

	if (s->type == NETWORK_TCP) {
		g_free(s->connection.tcp.local_name);
		for (l = g_list_first(s->connection.tcp.servers); l; l = l->next) {
			struct tcp_server *ts = l->data;
			freeaddrinfo(ts->addrinfo);
			g_free(ts->host);
			g_free(ts->port);
			g_free(ts->password);
			g_free(ts->name);
			g_free(ts);
		}
		g_list_free(s->connection.tcp.servers);
	} else if (s->type == NETWORK_PROGRAM) {
		xmlFree(s->connection.program.location);
	} else if (s->type == NETWORK_VIRTUAL) {
		xmlFree(s->connection.virtual.type);
	}

	for (l = s->autosend_lines; l; l = l->next) {
		g_free(l->data);
	}
	g_list_free(s->autosend_lines);
	
	g_free(s);

	return 0;
}


gboolean ping_loop(gpointer user_data) {
	GList *l = networks;
	while(l) {
		struct network *s = (struct network *)l->data;

		if(network_is_connected(s))network_send_args(s, "PING", s->name, NULL);
		else reconnect(s, TRUE);

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

gboolean network_change_nick(struct network *s, const char *nick)
{
	char *tmp, *p = NULL;

	if (!s) return FALSE;

	/* Change nick */
	if (!nick) nick = g_get_user_name();

	g_free(s->nick);
	s->nick = g_strdup(nick);

	/* Change hostmask */
	if (!s->hostmask) {
		s->hostmask = g_strdup_printf("%s!~%s@%s", nick, s->username, get_my_hostname());
	} else { 
		p = strchr(s->hostmask, '!');
		if (!p) return FALSE;
		tmp = g_strdup_printf("%s%s", nick, p);
		g_free(s->hostmask);
		s->hostmask = tmp;
	}
	return TRUE;
}

void register_virtual_network(struct virtual_network_ops *ops)
{
	if (!virtual_network_ops) {
		virtual_network_ops = g_hash_table_new(g_str_hash, g_str_equal);
	}
	g_hash_table_insert(virtual_network_ops, ops->name, ops);
}

void unregister_virtual_network(struct virtual_network_ops *ops)
{
	g_hash_table_remove(virtual_network_ops, ops->name);
}

gint ping_loop_id = -1;

gboolean init_networks() {
	GList *gl;

	if (ping_loop_id == -1) {
		ping_loop_id = g_timeout_add(1000 * 300, ping_loop, NULL);
	}

	for (gl = networks; gl; gl = gl->next)
	{
		struct network *n = gl->data;

		if (!network_is_connected(n) && n->autoconnect) {
#ifdef fork
			if(seperate_processes) { 
				if(fork() == 0) {  
					connect_network(n); 
					break; 
				}
			} else 
#endif
			{
				connect_network(n);
			}
		}
	}

	return TRUE;
}

GList *get_network_list() { return networks; }

void set_sslize_function (GIOChannel * (*f) (GIOChannel *)) 
{
	sslize_function = f;
}

struct network *find_network(const char *name)
{
	GList *gl;
	for (gl = networks; gl; gl = gl->next) {
		struct network *n = gl->data;
		if (n->name && !g_strcasecmp(n->name, name)) return n;
	}

	return NULL;
}

struct network *find_network_by_hostname(const char *hostname, guint16 port, gboolean create)
{
	GList *gl;
	struct network *n;
	
	char *portname = g_strdup_printf("%d", port);
	
	for (gl = get_network_list(); gl; gl = gl->next) {
		GList *sv;
		n = gl->data;
		
		if (n->type != NETWORK_TCP) continue;
		
		sv = n->connection.tcp.servers;

		while (sv) {
			struct tcp_server *server = sv->data;

			if (!g_strcasecmp(server->host, hostname) && !g_strcasecmp(server->port, portname)) {
				g_free(portname);
				return n;
			}

			sv = sv->next;
		} 
	}

	if ((n = find_network(hostname))) {
		g_free(portname);
		return n;
	}

	/* Create a new server */
	if (create)
	{
		struct addrinfo hints;
		struct tcp_server *s = g_new0(struct tcp_server, 1);
		int error;
		n = new_network();

		n->name = g_strdup(hostname);
		n->name_guessed = TRUE;
		n->type = NETWORK_TCP;
		s->host = g_strdup(hostname);
		s->port = portname;

		memset(&hints, 0, sizeof(hints));
		hints.ai_family = PF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;

		/* Lookup */
		error = getaddrinfo(s->host, s->port, &hints, &s->addrinfo);
		if (error) {
			log_network(NULL, n, "Unable to lookup %s:%s %s", s->host, s->port, gai_strerror(error));
			g_free(s->host);
			g_free(portname);
			g_free(s);
			close_network(n);
			return NULL;
		}

		n->connection.tcp.servers = g_list_append(n->connection.tcp.servers, s);

		return n;
	}

	return NULL;
}
