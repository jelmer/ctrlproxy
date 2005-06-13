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

static GIOChannel * (*sslize_function) (GIOChannel *, gboolean);

static GList *networks = NULL;

extern FILE *debugfd;

extern char *debugfile;

static GHashTable *virtual_network_ops = NULL;

static gboolean delayed_connect_server(struct network *s);
static gboolean connect_server(struct network *s);
static gboolean close_server(struct network *s);
static void reconnect(struct network *server, gboolean rm_source);

static void server_send_login (struct network *s) 
{
	g_assert(s->connection.state == NETWORK_CONNECTION_STATE_CONNECTED);

	log_network(NULL, s, "Successfully connected");

	s->state = new_network_state(s->config->nick, s->config->username, get_my_hostname());

	if(s->config->type == NETWORK_TCP && 
	   s->connection.data.tcp.current_server->password) { 
		network_send_args(s, "PASS", s->connection.data.tcp.current_server->password, NULL);
	} else if (s->config->password) {
		network_send_args(s, "PASS", s->config->password, NULL);
	}
	network_send_args(s, "NICK", s->config->nick, NULL);
	network_send_args(s, "USER", s->config->username, get_my_hostname(), s->config->name, s->config->fullname, NULL);

	s->connection.state = NETWORK_CONNECTION_STATE_LOGIN_SENT;
}

static gboolean process_from_server(struct network *n, struct line *l)
{
	struct line *lc;

	run_log_filter(n, lc = linedup(l), FROM_SERVER); free_line(lc);
	run_replication_filter(n, lc = linedup(l), FROM_SERVER); free_line(lc);

	state_handle_data(n->state,l);
	linestack_insert_line(n, l, FROM_SERVER);

	if(!g_strcasecmp(l->args[0], "PING")){
		network_send_args(n, "PONG", l->args[1], NULL);
		return TRUE;
	} else if(!g_strcasecmp(l->args[0], "PONG")){
		return TRUE;
	} else if(!g_strcasecmp(l->args[0], "ERROR")) {
		log_network(NULL, n, "error: %s", l->args[1]);
	} else if(!g_strcasecmp(l->args[0], "433") && 
			  n->connection.state == NETWORK_CONNECTION_STATE_LOGIN_SENT){
		char *old_nick = n->state->me.nick;
		n->state->me.nick = g_strdup_printf("%s_", n->state->me.nick);
		network_send_args(n, "NICK", n->state->me.nick, NULL);
		log_network(NULL, n, "%s was already in use, trying %s", old_nick, n->state->me.nick);
		g_free(old_nick);
	} else if(!g_strcasecmp(l->args[0], "422") ||
			  !g_strcasecmp(l->args[0], "376")) {
		GList *gl;
		n->connection.state = NETWORK_CONNECTION_STATE_MOTD_RECVD;

		server_connected_hook_execute(n);

		/* Rejoin channels */
		for (gl = n->config->channels; gl; gl = gl->next) 
		{
			struct channel_config *c = gl->data;
			if(c->autojoin) {
				network_send_args(n, "JOIN", c->name, c->key, NULL);
			} 
		}
	} 

	if(!l->dont_send && 
		n->connection.state == NETWORK_CONNECTION_STATE_MOTD_RECVD) {
		if (atoi(l->args[0])) {
			redirect_response(n, l);
		} else if (run_server_filter(n, l, FROM_SERVER)) {
			clients_send(n, l, NULL);
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

	if (server->connection.state == NETWORK_CONNECTION_STATE_CONNECTED) {
		server_send_login(server);
	}

	if (cond & G_IO_IN) {
		GError *err = NULL;
		GIOStatus status = irc_recv_line(c, &err, &l);

		if (status == G_IO_STATUS_ERROR) {
			log_network(NULL, server, 
					"Error \"%s\" reading from server, reconnecting in %ds...",
					err?err->message:"UNKNOWN", server->config->reconnect_interval);
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

		ret = process_from_server(server, l);

		free_line(l);

		return ret;
	}

	return TRUE;
}

static struct tcp_server_config *network_get_next_tcp_server(struct network *n)
{
	GList *cur = g_list_find(n->config->type_settings.tcp_servers, n->connection.data.tcp.current_server);

	/* Get next available server */
	if(cur && cur->next) cur = cur->next; else cur = n->config->type_settings.tcp_servers;

	if(cur) return cur->data; 

	return NULL;
}

gboolean network_send_line(struct network *s, const struct client *c, const struct line *ol)
{
	struct line l = *ol;
	struct line *lc;

	if (!run_server_filter(s, &l, TO_SERVER))
		return TRUE;

	run_log_filter(s, lc = linedup(&l), TO_SERVER); free_line(lc);
	run_replication_filter(s, lc = linedup(&l), TO_SERVER); free_line(lc);
	linestack_insert_line(s, ol, TO_SERVER);

	/* Also write this message to all other clients currently connected */
	if(!l.is_private && l.args[0] &&
	   (!strcmp(l.args[0], "PRIVMSG") || !strcmp(l.args[0], "NOTICE"))) {
		clients_send(s, &l, c);
	}

	l.origin = NULL;		/* Never send origin to the server */

	switch (s->config->type) {
	case NETWORK_TCP:
		return irc_send_line(s->connection.data.tcp.outgoing, &l);

	case NETWORK_PROGRAM:
		return irc_send_line(s->connection.data.program.outgoing, &l);

	case NETWORK_VIRTUAL:
		if (!s->connection.data.virtual.ops) 
			return FALSE;
		return s->connection.data.virtual.ops->to_server(s, c, &l);

	default:
		g_assert(0);
		return FALSE;
	}
}

gboolean virtual_network_recv_line(struct network *s, struct line *l)
{
	if (!l->origin) 
		l->origin = g_strdup(get_my_hostname());

	return process_from_server(s, l);
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
	l->origin = g_strdup(s->state->me.nick);
	va_end(ap);

	ret = network_send_line(s, NULL, l);

	free_line(l);

	return ret;
}

static gboolean connect_current_tcp_server(struct network *s) 
{
	struct addrinfo *res;
	int sock = -1;
	size_t size;
	struct tcp_server_config *cs;
	GIOChannel *ioc = NULL;
	struct addrinfo hints;
	struct addrinfo *addrinfo;
	int error;

	if (!s->connection.data.tcp.current_server) {
		s->connection.data.tcp.current_server = network_get_next_tcp_server(s);
	}
	
	cs = s->connection.data.tcp.current_server;
	if(!cs) {
		s->config->autoconnect = FALSE;
		log_network(NULL, s, "No servers listed, not connecting");
		return FALSE;
	}

	log_network(NULL, s, "Connecting with %s:%s", 
			  cs->host, 
			  cs->port);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	/* Lookup */
	error = getaddrinfo(cs->host, cs->port, &hints, &addrinfo);
	if (error) {
		log_network(NULL, s, "Unable to lookup %s:%s %s", cs->host, cs->port, gai_strerror(error));
		return FALSE;
	}

	/* Connect */

	for (res = addrinfo; res; res = res->ai_next) {
		sock = socket(res->ai_family, res->ai_socktype,
					  res->ai_protocol);
		if (sock < 0) {
			continue;
		}

		ioc = g_io_channel_unix_new(sock);
		if (connect(sock, res->ai_addr, res->ai_addrlen) < 0 && errno != EINPROGRESS) {
			g_io_channel_unref(ioc);
			ioc = NULL;
			continue;
		}

		break; 
	}

	freeaddrinfo(addrinfo);

	if (!ioc) {
		log_network(NULL, s, "Unable to connect: %s", strerror(errno));
		return FALSE;
	}

	s->connection.state = NETWORK_CONNECTION_STATE_CONNECTED;

	size = sizeof(struct sockaddr_in6);
	g_free(s->connection.data.tcp.local_name);
	s->connection.data.tcp.remote_name = g_malloc(size);
	s->connection.data.tcp.local_name = g_malloc(size);
	s->connection.data.tcp.namelen = getsockname(sock, s->connection.data.tcp.local_name, &size);
	getpeername(sock, s->connection.data.tcp.remote_name, &size);

	if (cs->ssl) {
		GIOChannel *nio = sslize(ioc, FALSE);

		if (!nio) {
			log_network(NULL, s, "SSL enabled for %s:%s, but no SSL support loaded", cs->host, cs->port);
		} else {
			ioc = nio;
		}
	}

	g_io_channel_set_close_on_unref(ioc, TRUE);

	g_io_channel_set_flags(ioc, G_IO_FLAG_NONBLOCK, NULL);
	g_io_channel_set_encoding(ioc, NULL, NULL);

	s->connection.data.tcp.outgoing = ioc;
	
	s->connection.data.tcp.outgoing_id = g_io_add_watch(s->connection.data.tcp.outgoing, G_IO_IN | G_IO_HUP | G_IO_ERR, handle_server_receive, s);

	g_io_channel_unref(s->connection.data.tcp.outgoing);

	if(!s->name && cs->name) {
		s->name = g_strdup(cs->name);
	}

	if(!s->connection.data.tcp.outgoing) {
		log_network(NULL, s, "Couldn't connect via server %s:%s", cs->host, cs->port);
		return FALSE;
	}

	return TRUE;
}

static void reconnect(struct network *server, gboolean rm_source)
{
	g_assert(server->connection.state != NETWORK_CONNECTION_STATE_RECONNECT_PENDING);

	if (server->connection.state == NETWORK_CONNECTION_STATE_MOTD_RECVD) {
		server_disconnected_hook_execute(server);
	}

	close_server(server);

	if (server->config->type == NETWORK_TCP) {
		server->connection.state = NETWORK_CONNECTION_STATE_RECONNECT_PENDING;
		server->connection.data.tcp.current_server = network_get_next_tcp_server(server);
		server->reconnect_id = g_timeout_add(1000 * server->config->reconnect_interval, (GSourceFunc) delayed_connect_server, server);
	} else {
		connect_server(server);	
	}
}

static gboolean close_server(struct network *n) 
{

	if(n->connection.state == NETWORK_CONNECTION_STATE_RECONNECT_PENDING) {
		g_source_remove(n->reconnect_id);
		n->connection.state = NETWORK_CONNECTION_STATE_NOT_CONNECTED;
	}

	if(n->connection.state == NETWORK_CONNECTION_STATE_NOT_CONNECTED) {
		return FALSE;
	} 

	network_send_args(n, "QUIT", NULL);

	if (n->connection.state == NETWORK_CONNECTION_STATE_MOTD_RECVD) {
		server_disconnected_hook_execute(n);
	}

	if (n->state) {
		free_network_state(n->state); 
		n->state = NULL;
	}

	switch (n->config->type) {
	case NETWORK_TCP: 
		g_source_remove(n->connection.data.tcp.outgoing_id); 
		break;
	case NETWORK_PROGRAM: 
		g_source_remove(n->connection.data.program.outgoing_id); 
		break;
	case NETWORK_VIRTUAL:
		if (n->connection.data.virtual.ops && n->connection.data.virtual.ops->fini) {
			n->connection.data.virtual.ops->fini(n);
		}
		break;
	default: g_assert(0);
	}

	n->connection.state = NETWORK_CONNECTION_STATE_NOT_CONNECTED;

	return TRUE;
}

void clients_send(struct network *server, struct line *l, const struct client *exception) 
{
	GList *g;
	for (g = server->clients; g; g = g->next) {
		struct client *c = (struct client *)g->data;
		if(c != exception) {
			if(run_client_filter(c, l, FROM_SERVER)) { 
				client_send_line(c, l);
			}
		}
	}
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
	char *cmd[2] = { s->config->type_settings.program_location, NULL };
	pid_t pid = piped_child(cmd, &sock);

	if (pid == -1) return FALSE;

	s->connection.data.program.outgoing = g_io_channel_unix_new(sock);
	g_io_channel_set_close_on_unref(s->connection.data.program.outgoing, TRUE);
	s->connection.state = NETWORK_CONNECTION_STATE_CONNECTED;

	server_send_login(s);
	
	s->connection.data.program.outgoing_id = g_io_add_watch(s->connection.data.program.outgoing, G_IO_IN | G_IO_ERR, handle_server_receive, s);

	g_io_channel_unref(s->connection.data.program.outgoing);

	if(!s->name) {
		if (strchr(s->config->type_settings.program_location, '/')) {
			s->name = g_strdup(strrchr(s->config->type_settings.program_location, '/')+1);
		} else {
			s->name = g_strdup(s->config->type_settings.program_location);
		}
	}

	return TRUE;
}

static gboolean connect_server(struct network *s)
{
	switch (s->config->type) {
	case NETWORK_TCP:
		return connect_current_tcp_server(s);

	case NETWORK_PROGRAM:
		return connect_program(s);

	case NETWORK_VIRTUAL:
		s->connection.data.virtual.ops = g_hash_table_lookup(virtual_network_ops, s->config->type_settings.virtual_type);
		if (!s->connection.data.virtual.ops) return FALSE;

		s->state = new_network_state(s->config->nick, s->config->username, get_my_hostname());
    	s->connection.state = NETWORK_CONNECTION_STATE_MOTD_RECVD;

		if (s->connection.data.virtual.ops->init)
			return s->connection.data.virtual.ops->init(s);

		/* FIXME: Set s->connection.virtual.ops->send */

		return TRUE;
	default: g_assert(0);
	}

	return TRUE;
}

static gboolean delayed_connect_server(struct network *s)
{
	connect_server(s);
	return (s->connection.state == NETWORK_CONNECTION_STATE_RECONNECT_PENDING);
}


struct network *load_network(struct network_config *sc)
{
	struct network *s;

	/* Don't connect to the same network twice */
	s = find_network(sc->name);
	if (s) 
		return s;

	s = g_new0(struct network, 1);
	s->config = sc;
	s->name = g_strdup(s->config->name);

	networks = g_list_append(networks, s);
	return s;
}

/* Connect to a network, returns TRUE if connection was successful 
 * (or startup of connection was successful) */
gboolean connect_network(struct network *s) 
{
	return connect_server(s);
}

void unload_network(struct network *s)
{
	GList *l = s->clients;
	g_assert(s);
	if (s->connection.state == NETWORK_CONNECTION_STATE_MOTD_RECVD) {
		log_network(NULL, s, "Closing connection");
	}

	while(l) {
		struct client *c = l->data;
		l = l->next;
		disconnect_client(c, "Server exiting");
	}

	networks = g_list_remove(networks, s);

	g_free(s->name);

	if (s->config->type == NETWORK_TCP) {
		g_free(s->connection.data.tcp.local_name);
		g_free(s->connection.data.tcp.remote_name);
	}

	g_free(s);
}

gboolean disconnect_network(struct network *s)
{
	return close_server(s);
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

void register_virtual_network(struct virtual_network_ops *ops)
{
	g_hash_table_insert(virtual_network_ops, ops->name, ops);
}

void unregister_virtual_network(struct virtual_network_ops *ops)
{
	g_hash_table_remove(virtual_network_ops, ops->name);
}

gboolean init_networks(void)
{
	virtual_network_ops = g_hash_table_new(g_str_hash, g_str_equal);
	return TRUE;
}

gboolean autoconnect_networks(void)
{
	GList *gl;
	for (gl = networks; gl; gl = gl->next)
	{
		struct network *n = gl->data;
		if (n->config->autoconnect) {
#ifdef HAVE_FORK
			if(get_current_config()->separate_processes) { 
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

gboolean load_networks(struct ctrlproxy_config *cfg)
{
	GList *gl;
	for (gl = cfg->networks; gl; gl = gl->next)
	{
		struct network_config *nc = gl->data;
		load_network(nc);
	}

	return TRUE;
}

GList *get_network_list() { return networks; }

void set_sslize_function (GIOChannel * (*f) (GIOChannel *, gboolean)) 
{
	sslize_function = f;
}

GIOChannel *sslize (GIOChannel *orig, gboolean server)
{
	if (!sslize_function)
		return NULL;

	g_io_channel_set_close_on_unref(orig, TRUE);
	g_io_channel_set_encoding(orig, NULL, NULL);

	return sslize_function(orig, server);
}

struct network *find_network(const char *name)
{
	GList *gl;
	for (gl = networks; gl; gl = gl->next) {
		struct network *n = gl->data;
		if (n->name && !strcmp(n->name, name)) return n;
	}

	return NULL;
}

struct network *find_network_by_hostname(const char *hostname, guint16 port, gboolean create)
{
	GList *gl;
	struct network *n;
	
	char *portname = g_strdup_printf("%d", port);
	
	for (gl = networks; gl; gl = gl->next) {
		GList *sv;
		n = gl->data;

		if (n->name && !strcmp(n->name, hostname)) {
			g_free(portname);
			return n;
		}
		
		if (n->config->type != NETWORK_TCP) continue;
		
		sv = n->config->type_settings.tcp_servers;

		while (sv) {
			struct tcp_server_config *server = sv->data;

			if (!g_strcasecmp(server->host, hostname) && !g_strcasecmp(server->port, portname)) {
				g_free(portname);
				return n;
			}

			sv = sv->next;
		} 
	}

	/* Create a new server */
	if (create)
	{
		struct tcp_server_config *s = g_new0(struct tcp_server_config, 1);
		struct network_config *nc;
		nc = new_network_config(get_current_config());

		nc->name = g_strdup(hostname);
		nc->type = NETWORK_TCP;
		s->host = g_strdup(hostname);
		s->port = portname;

		nc->type_settings.tcp_servers = g_list_append(nc->type_settings.tcp_servers, s);

		return load_network(nc);
	}

	return NULL;
}

void fini_networks()
{
	GList *gl;
	while((gl = get_network_list())) {
		struct network *n = (struct network *)gl->data;
		disconnect_network(n);
		unload_network(n);
	}
}

void network_select_next_server(struct network *n)
{
	if (n->config->type != NETWORK_TCP) return;
	log_network(NULL, n, "Trying next server");
	n->connection.data.tcp.current_server = network_get_next_tcp_server(n);
}
