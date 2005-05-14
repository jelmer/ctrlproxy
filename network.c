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
	g_assert(s->state == NETWORK_STATE_CONNECTED);

	log_network(NULL, s, "Successfully connected");

	if(s->type == NETWORK_TCP && s->connection.tcp.current_server->password) { 
		network_send_args(s, "PASS", s->connection.tcp.current_server->password, NULL);
	} else if (s->password) {
		network_send_args(s, "PASS", s->password, NULL);
	}
	network_send_args(s, "NICK", s->nick, NULL);
	network_send_args(s, "USER", s->username, get_my_hostname(), s->name, s->fullname, NULL);

	s->state = NETWORK_STATE_LOGIN_SENT;
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
		return TRUE;
	} else if(!g_strcasecmp(l->args[0], "PONG")){
		return TRUE;
	} else if(!g_strcasecmp(l->args[0], "ERROR")) {
		log_network(NULL, l->network, "error: %s", l->args[1]);
	} else if(!g_strcasecmp(l->args[0], "433") && 
			  l->network->state == NETWORK_STATE_LOGIN_SENT){
		char *old_nick = l->network->nick;
		l->network->nick = g_strdup_printf("%s_", l->network->nick);
		network_send_args(l->network, "NICK", l->network->nick, NULL);
		g_free(old_nick);
	} else if(!g_strcasecmp(l->args[0], "422") ||
			  !g_strcasecmp(l->args[0], "376")) {
		GList *gl;
		l->network->state = NETWORK_STATE_MOTD_RECVD;

		server_connected_hook_execute(l->network);

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
	} 

	if(!(l->options & LINE_DONT_SEND) && 
		l->network->state == NETWORK_STATE_MOTD_RECVD) {
		if (atoi(l->args[0])) {
			redirect_response(l->network, l);
		} else if (run_server_filter(l)) {
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

	if ((cond & G_IO_HUP)) {
		log_network(NULL, server, "Hangup from server, scheduling reconnect");
		reconnect(server, FALSE);
		return FALSE;
	}

	if ((cond & G_IO_ERR)) {
		log_network(NULL, server, "Error from server, scheduling reconnect...");
		reconnect(server, FALSE);
		return FALSE;
	}

	if (server->state == NETWORK_STATE_CONNECTED) {
		server_send_login(server);
	}

	if (cond & G_IO_IN) {
		GError *err = NULL;
		GIOStatus status = irc_recv_line(c, &err, &l);

		if (status == G_IO_STATUS_ERROR) {
			log_network(NULL, server, 
					"Error \"%s\" reading from server, reconnecting in %ds...",
					err?err->message:"UNKNOWN", server->reconnect_interval);
			reconnect(server, FALSE);
			return FALSE;
		}
		
		if(status == G_IO_STATUS_AGAIN || 
		   status == G_IO_STATUS_EOF || !l) return TRUE;

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

gboolean network_send_line(struct network *s, const struct line *ol)
{
	struct line l = *ol;
	struct line *lc;

	l.origin = NULL;		/* Never send origin to the server */
	l.network = s;

	state_handle_data(s, &l);

	if (!run_server_filter(&l))
		return TRUE;

	run_log_filter(lc = linedup(&l)); free_line(lc);
	run_replication_filter(lc = linedup(&l)); free_line(lc);

	/* Also write this message to all other clients currently connected */
	if(!(l.options & LINE_IS_PRIVATE) && l.args[0] &&
	   (!strcmp(l.args[0], "PRIVMSG") || !strcmp(l.args[0], "NOTICE"))) {
		char *old_origin;
		old_origin = l.origin; l.origin = s->nick;
		clients_send(s, &l, l.client);
		l.origin = old_origin;
	}

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
		g_assert(0);
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
	struct tcp_server *cs;
	GIOChannel *ioc = NULL;

	if (!s->connection.tcp.current_server) {
		s->connection.tcp.current_server = network_get_next_tcp_server(s);
	}
	
	cs = s->connection.tcp.current_server;
	if(!cs) {
		s->autoconnect = FALSE;
		log_network(NULL, s, "No servers listed, not connecting");
		return FALSE;
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
		g_io_channel_set_close_on_unref(ioc, TRUE);

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

	s->state = NETWORK_STATE_CONNECTED;

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
		return FALSE;
	}

	return TRUE;
}

static gboolean delayed_connect_network(struct network *s)
{
	connect_network(s);
	return (s->state == NETWORK_STATE_RECONNECT_PENDING);
}

static void reconnect(struct network *server, gboolean rm_source)
{
	GList *gl;
	g_assert(server->state != NETWORK_STATE_RECONNECT_PENDING);

	if (server->state == NETWORK_STATE_MOTD_RECVD) {
		server_disconnected_hook_execute(server);
	}

	if (server->state != NETWORK_STATE_NOT_CONNECTED) {
		switch (server->type) {
		case NETWORK_TCP: 
			if (rm_source) g_source_remove(server->connection.tcp.outgoing_id); 
			server->connection.tcp.outgoing = NULL; 
			break;
		case NETWORK_PROGRAM: 
			if (rm_source) g_source_remove(server->connection.program.outgoing_id); 
			server->connection.program.outgoing = NULL; 
			break;
		default:break;
		}
	}

	server->state = NETWORK_STATE_NOT_CONNECTED;
	
	for (gl = server->channels; gl; gl = gl->next) {
		struct channel *c = gl->data;
		c->joined = FALSE;
	}

	if (server->type == NETWORK_TCP) {
		server->state = NETWORK_STATE_RECONNECT_PENDING;
		server->connection.tcp.current_server = network_get_next_tcp_server(server);
		server->reconnect_id = g_timeout_add(1000 * server->reconnect_interval, (GSourceFunc) delayed_connect_network, server);
	} else {
		connect_network(server);	
	}
}

gboolean close_server(struct network *n) 
{
	int i;

	if(n->state == NETWORK_STATE_RECONNECT_PENDING) {
		g_source_remove(n->reconnect_id);
		n->state = NETWORK_STATE_NOT_CONNECTED;
	}

	if(n->state == NETWORK_STATE_NOT_CONNECTED) {
		return FALSE;
	} 

	network_send_args(n, "QUIT", NULL);
	if (n->state == NETWORK_STATE_MOTD_RECVD) {
		server_disconnected_hook_execute(n);
	}

	switch (n->type) {
	case NETWORK_TCP: 
		g_source_remove(n->connection.tcp.outgoing_id); 
		break;
	case NETWORK_PROGRAM: 
		g_source_remove(n->connection.program.outgoing_id); 
		break;
	case NETWORK_VIRTUAL:
		if (n->connection.virtual.ops && n->connection.virtual.ops->fini) {
			n->connection.virtual.ops->fini(n);
		}
		break;
	default: g_assert(0);
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

	n->state = NETWORK_STATE_NOT_CONNECTED;

	return TRUE;
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
	struct network *s = g_new0(struct network, 1);

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
	g_io_channel_set_close_on_unref(s->connection.program.outgoing, TRUE);
	s->state = NETWORK_STATE_CONNECTED;

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

/* Connect to a network, returns TRUE if connection was successful 
 * (or startup of connection was successful) */
gboolean connect_network(struct network *s) 
{
	if (s->state == NETWORK_STATE_RECONNECT_PENDING) 
	{
		g_source_remove(s->reconnect_id);
		s->state = NETWORK_STATE_NOT_CONNECTED;
	}

	g_assert(s->state == NETWORK_STATE_NOT_CONNECTED);
	
	switch (s->type) {
	case NETWORK_TCP:
		return connect_current_tcp_server(s);

	case NETWORK_PROGRAM:
		return connect_program(s);

	case NETWORK_VIRTUAL:
		s->connection.virtual.ops = g_hash_table_lookup(virtual_network_ops, s->connection.virtual.type);
		if (!s->connection.virtual.ops) return FALSE;

		if (s->connection.virtual.ops->init) 
			return s->connection.virtual.ops->init(s);

		/* FIXME: Set s->connection.virtual.ops->send */

		return TRUE;
	default: g_assert(0);
	}

	return TRUE;
}

int close_network(struct network *s)
{
	GList *l = s->clients;
	g_assert(s);
	if (s->state == NETWORK_STATE_MOTD_RECVD) {
		log_network(NULL, s, "Closing connection");
	}

	while(l) {
		struct client *c = l->data;
		l = l->next;
		disconnect_client(c, "Server exiting");
	}
	
	g_list_free(s->clients); s->clients = NULL;

	close_server(s);

	networks = g_list_remove(networks, s);

	if (s->state == NETWORK_STATE_RECONNECT_PENDING) {
		g_source_remove(s->reconnect_id);
	}

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

gboolean init_networks() {
	GList *gl;

	for (gl = networks; gl; gl = gl->next)
	{
		struct network *n = gl->data;

		if (n->state == NETWORK_STATE_NOT_CONNECTED && n->autoconnect) {
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
