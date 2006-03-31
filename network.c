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

static GIOChannel * (*sslize_function) (GIOChannel *, gboolean);

static GHashTable *virtual_network_ops = NULL;

static gboolean delayed_connect_server(struct network *s);
static gboolean connect_server(struct network *s);
static gboolean close_server(struct network *s);
static void reconnect(struct network *server, gboolean rm_source);

static void server_send_login (struct network *s) 
{
	g_assert(s);

	g_assert(s->connection.state == NETWORK_CONNECTION_STATE_CONNECTED);
	s->connection.state = NETWORK_CONNECTION_STATE_LOGIN_SENT;

	log_network(NULL, LOG_TRACE, s, "Sending login details");

	s->state = network_state_init(&s->info, s->config->nick, s->config->username, get_my_hostname());

	if(s->config->type == NETWORK_TCP && 
	   s->connection.data.tcp.current_server->password) { 
		network_send_args(s, "PASS", s->connection.data.tcp.current_server->password, NULL);
	} else if (s->config->password) {
		network_send_args(s, "PASS", s->config->password, NULL);
	}
	network_send_args(s, "NICK", s->config->nick, NULL);
	network_send_args(s, "USER", s->config->username, get_my_hostname(), s->config->name, s->config->fullname, NULL);
}

static gboolean process_from_server(struct network *n, struct line *l)
{
	struct line *lc;

	g_assert(n);
	g_assert(l);

	run_log_filter(n, lc = linedup(l), FROM_SERVER); free_line(lc);
	run_replication_filter(n, lc = linedup(l), FROM_SERVER); free_line(lc);

	g_assert(n->state);

	state_handle_data(n->state,l);
	linestack_insert_line(n->global->linestack, n, l, FROM_SERVER);

	g_assert(l->args[0]);

	if(!g_strcasecmp(l->args[0], "PING")){
		network_send_args(n, "PONG", l->args[1], NULL);
		return TRUE;
	} else if(!g_strcasecmp(l->args[0], "PONG")){
		return TRUE;
	} else if(!g_strcasecmp(l->args[0], "ERROR")) {
		log_network(NULL, LOG_ERROR, n, "error: %s", l->args[1]);
	} else if(!g_strcasecmp(l->args[0], "433") && 
			  n->connection.state == NETWORK_CONNECTION_STATE_LOGIN_SENT){
		char *tmp = g_strdup_printf("%s_", l->args[2]);
		network_send_args(n, "NICK", tmp, NULL);
		log_network(NULL, LOG_WARNING, n, "%s was already in use, trying %s", l->args[2], tmp);
		g_free(tmp);
	} else if(!g_strcasecmp(l->args[0], "422") ||
			  !g_strcasecmp(l->args[0], "376")) {
		GList *gl;
		n->connection.state = NETWORK_CONNECTION_STATE_MOTD_RECVD;

		log_network(NULL, LOG_INFO, n, "Successfully logged in");
		server_connected_hook_execute(n);

		network_send_args(n, "USERHOST", n->state->me.nick, NULL);

		/* Rejoin channels */
		for (gl = n->config->channels; gl; gl = gl->next) 
		{
			struct channel_config *c = gl->data;
			if(c->autojoin) {
				network_send_args(n, "JOIN", c->name, c->key, NULL);
			} 
		}
	} 

	if( n->connection.state == NETWORK_CONNECTION_STATE_MOTD_RECVD) {
		if (atoi(l->args[0])) {
			redirect_response(n, l);
		} else if(!g_strcasecmp(l->args[0], "PRIVMSG") && l->args[2][0] == '\001') {
			ctcp_process(n, l);
		} else if (run_server_filter(n, l, FROM_SERVER)) {
			clients_send(n, l, NULL);
		} 
	} 

	return TRUE;
}

static gboolean handle_server_receive (GIOChannel *c, GIOCondition cond, void *_server)
{
	struct network *server = (struct network *)_server;
	struct line *l;
	gboolean ret;

	g_assert(c);
	g_assert(server);

	if ((cond & G_IO_HUP)) {
		log_network(NULL, LOG_WARNING, server, "Hangup from server, scheduling reconnect");
		reconnect(server, FALSE);
		return FALSE;
	}

	if ((cond & G_IO_ERR)) {
		log_network(NULL, LOG_WARNING, server, "Error from server, scheduling reconnect");
		reconnect(server, FALSE);
		return FALSE;
	}


	if (cond & G_IO_IN) {
		GError *err = NULL;
		GIOStatus status = irc_recv_line(c, &err, &l);

		log_network_line(server, l, TRUE);

		if (status == G_IO_STATUS_ERROR) {
			log_network(NULL, LOG_WARNING, server, 
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

static gboolean handle_server_connected (GIOChannel *c, GIOCondition cond, void *_server)
{
	struct network *server = (struct network *)_server;

	g_assert(server);

	server->connection.state = NETWORK_CONNECTION_STATE_CONNECTED;
	server_send_login(server);

	server->connection.data.tcp.outgoing_id = g_io_add_watch(server->connection.data.tcp.outgoing, G_IO_IN | G_IO_HUP | G_IO_ERR, handle_server_receive, server);
	
	return FALSE;
}



static struct tcp_server_config *network_get_next_tcp_server(struct network *n)
{
	GList *cur;
	
	g_assert(n);
	g_assert(n->config);
	cur = g_list_find(n->config->type_settings.tcp_servers, n->connection.data.tcp.current_server);

	/* Get next available server */
	if(cur && cur->next) 
		cur = cur->next; 
	else 
		cur = n->config->type_settings.tcp_servers;

	if(cur) return cur->data; 

	return NULL;
}

gboolean network_send_line(struct network *s, const struct client *c, const struct line *ol)
{
	struct line l;
	struct line *lc;

	g_assert(ol);
	g_assert(s);
	l = *ol;

	if (!run_server_filter(s, &l, TO_SERVER))
		return TRUE;

	run_log_filter(s, lc = linedup(&l), TO_SERVER); free_line(lc);
	run_replication_filter(s, lc = linedup(&l), TO_SERVER); free_line(lc);
	linestack_insert_line(s->global->linestack, s, ol, TO_SERVER);

	g_assert(l.args[0]);
	/* Also write this message to all other clients currently connected */
	if(!l.is_private && l.args[0] &&
	   (!g_strcasecmp(l.args[0], "PRIVMSG") || 
		!g_strcasecmp(l.args[0], "NOTICE")) && 
	   (l.argc <= 2 || l.args[2][0] != '\001')) {
		clients_send(s, &l, c);
	}

	l.origin = NULL;		/* Never send origin to the server */

	log_network_line(s, &l, FALSE);

	redirect_record(s, c, &l);

	g_assert(s->config);
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
	g_assert(s);
	g_assert(l);

	if (!l->origin) 
		l->origin = g_strdup(get_my_hostname());

	return process_from_server(s, l);
}

gboolean virtual_network_recv_args(struct network *s, const char *origin, ...)
{
	va_list ap;
	struct line *l;
	gboolean ret;

	g_assert(s);

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

	g_assert(s);

	if (!s->state) 
		return FALSE;

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
	struct addrinfo *addrinfo = NULL;
	int error;

	g_assert(s);

	if (!s->connection.data.tcp.current_server) {
		s->connection.data.tcp.current_server = network_get_next_tcp_server(s);
	}

	log_network(NULL, LOG_TRACE, s, "connect_current_tcp_server");
	
	cs = s->connection.data.tcp.current_server;
	if(!cs) {
		s->config->autoconnect = FALSE;
		log_network(NULL, LOG_WARNING, s, "No servers listed, not connecting");
		return FALSE;
	}

	log_network(NULL, LOG_INFO, s, "Connecting with %s:%s", 
			  cs->host, 
			  cs->port);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	/* Lookup */
	error = getaddrinfo(cs->host, cs->port, &hints, &addrinfo);
	if (error) {
		log_network(NULL, LOG_ERROR, s, "Unable to lookup %s:%s %s", cs->host, cs->port, gai_strerror(error));
		freeaddrinfo(addrinfo);
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
		log_network(NULL, LOG_ERROR, s, "Unable to connect: %s", strerror(errno));
		return FALSE;
	}

	size = sizeof(struct sockaddr_in6);
	g_free(s->connection.data.tcp.local_name);
	s->connection.data.tcp.remote_name = g_malloc(size);
	s->connection.data.tcp.local_name = g_malloc(size);
	s->connection.data.tcp.namelen = getsockname(sock, s->connection.data.tcp.local_name, &size);
	getpeername(sock, s->connection.data.tcp.remote_name, &size);

	if (cs->ssl) {
		GIOChannel *nio = sslize(ioc, FALSE);

		if (!nio) {
			log_network(NULL, LOG_WARNING, s, "SSL enabled for %s:%s, but no SSL support loaded", cs->host, cs->port);
		} else {
			ioc = nio;
		}
	}

	if(!ioc) {
		log_network(NULL, LOG_ERROR, s, "Couldn't connect via server %s:%s", cs->host, cs->port);
		return FALSE;
	}

	g_io_channel_set_close_on_unref(ioc, TRUE);

	g_io_channel_set_flags(ioc, G_IO_FLAG_NONBLOCK, NULL);
	g_io_channel_set_encoding(ioc, NULL, NULL);

	s->connection.data.tcp.outgoing = ioc;
	
	s->connection.data.tcp.outgoing_id = g_io_add_watch(s->connection.data.tcp.outgoing, G_IO_OUT, handle_server_connected, s);

	g_io_channel_unref(s->connection.data.tcp.outgoing);

	return TRUE;
}

static void reconnect(struct network *server, gboolean rm_source)
{
	g_assert(server);

	g_assert(server->connection.state != NETWORK_CONNECTION_STATE_RECONNECT_PENDING);

	if (server->connection.state == NETWORK_CONNECTION_STATE_MOTD_RECVD) {
		server_disconnected_hook_execute(server);
	}

	close_server(server);

	g_assert(server->config);

	if (server->config->type == NETWORK_TCP)
		server->connection.data.tcp.current_server = network_get_next_tcp_server(server);

	if (server->config->type == NETWORK_TCP ||
		server->config->type == NETWORK_PROGRAM) {
		server->connection.state = NETWORK_CONNECTION_STATE_RECONNECT_PENDING;
		server->reconnect_id = g_timeout_add(1000 * server->config->reconnect_interval, (GSourceFunc) delayed_connect_server, server);
	} else {
		connect_server(server);	
	}
}

static gboolean close_server(struct network *n) 
{
	g_assert(n);

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
		network_update_config(n->state, n->config);
		free_network_state(n->state); 
		n->state = NULL;
	}

	g_assert(n->config);

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
	redirect_clear(n);

	return TRUE;
}

void clients_send(struct network *server, struct line *l, const struct client *exception) 
{
	GList *g;

	g_assert(server);

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
		log_global(NULL, LOG_ERROR, "socketpair: %s", strerror(errno));
		return -1;
	}

	*f_in = sock[0];

	fcntl(sock[0], F_SETFL, O_NONBLOCK);

	pid = fork();

	if (pid == -1) {
		log_global(NULL, LOG_ERROR, "fork: %s", strerror(errno));
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
		exit(-1);
	}

	close(sock[1]);

	return pid;
}

static gboolean connect_program(struct network *s)
{
	int sock;
	char *cmd[2];
	pid_t pid;
	
	g_assert(s);
	g_assert(s->config);
	g_assert(s->config->type == NETWORK_PROGRAM);
	
	cmd[0] = s->config->type_settings.program_location;
	cmd[1] = NULL;
	pid = piped_child(cmd, &sock);

	if (pid == -1) return FALSE;

	s->connection.data.program.outgoing = g_io_channel_unix_new(sock);
	g_io_channel_set_close_on_unref(s->connection.data.program.outgoing, TRUE);
	s->connection.state = NETWORK_CONNECTION_STATE_CONNECTED;

	server_send_login(s);
	
	s->connection.data.program.outgoing_id = g_io_add_watch(s->connection.data.program.outgoing, G_IO_IN | G_IO_HUP | G_IO_ERR, handle_server_receive, s);

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
	g_assert(s);
	g_assert(s->config);

	switch (s->config->type) {
	case NETWORK_TCP:
		return connect_current_tcp_server(s);

	case NETWORK_PROGRAM:
		return connect_program(s);

	case NETWORK_VIRTUAL:
		s->connection.data.virtual.ops = g_hash_table_lookup(virtual_network_ops, s->config->type_settings.virtual_type);
		if (!s->connection.data.virtual.ops) return FALSE;

		s->state = network_state_init(&s->info, s->config->nick, s->config->username, get_my_hostname());
    	s->connection.state = NETWORK_CONNECTION_STATE_MOTD_RECVD;

		if (s->connection.data.virtual.ops->init)
			return s->connection.data.virtual.ops->init(s);

		return TRUE;
	default: g_assert(0);
	}

	return TRUE;
}

static gboolean delayed_connect_server(struct network *s)
{
	g_assert(s);
	connect_server(s);
	return (s->connection.state == NETWORK_CONNECTION_STATE_RECONNECT_PENDING);
}


struct network *load_network(struct global *global, struct network_config *sc)
{
	struct network *s;

	g_assert(sc);

	/* Don't connect to the same network twice */
	s = find_network(global, sc->name);
	if (s) 
		return s;

	s = g_new0(struct network, 1);
	s->config = sc;
	s->info.features = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	s->name = g_strdup(s->config->name);
	s->global = global;

	global->networks = g_list_append(global->networks, s);
	return s;
}

/* Connect to a network, returns TRUE if connection was successful 
 * (or startup of connection was successful) */
gboolean connect_network(struct network *s) 
{
	g_assert(s);
	return connect_server(s);
}

void unload_network(struct network *s)
{
	GList *l;
	
	g_assert(s);
	l = s->clients;

	if (s->connection.state == NETWORK_CONNECTION_STATE_MOTD_RECVD) {
		log_network(NULL, LOG_INFO, s, "Closing connection");
	}

	while(l) {
		struct client *c = l->data;
		l = l->next;
		disconnect_client(c, "Server exiting");
	}

	s->global->networks = g_list_remove(s->global->networks, s);

	g_free(s->name);

	if (s->config->type == NETWORK_TCP) {
		g_free(s->connection.data.tcp.local_name);
		g_free(s->connection.data.tcp.remote_name);
	}

	g_free(s->info.supported_user_modes);
	g_free(s->info.supported_channel_modes);
	g_free(s->info.server);
	g_free(s->info.name);

	g_hash_table_destroy(s->info.features);
	g_free(s);
}

gboolean disconnect_network(struct network *s)
{
	g_assert(s);
	if(s->connection.state == NETWORK_CONNECTION_STATE_NOT_CONNECTED) {
		return FALSE;
	}
	log_network(NULL, LOG_INFO, s, "Disconnecting");
	return close_server(s);
}

int verify_client(const struct network *s, const struct client *c)
{
	GList *gl;

	g_assert(s);
	g_assert(c);
	
	gl = s->clients;
	while(gl) {
		struct client *nc = (struct client *)gl->data;
		if(c == nc)return 1;
		gl = gl->next;
	}

	return 0;
}

void register_virtual_network(struct virtual_network_ops *ops)
{
	if (virtual_network_ops == NULL)
		virtual_network_ops = g_hash_table_new(g_str_hash, g_str_equal);
	g_assert(ops);
	g_hash_table_insert(virtual_network_ops, ops->name, ops);
}

gboolean autoconnect_networks(struct global *global)
{
	GList *gl;
	for (gl = global->networks; gl; gl = gl->next)
	{
		struct network *n = gl->data;
		g_assert(n);
		g_assert(n->config);
		if (n->config->autoconnect) {
			connect_network(n);
		}
	}

	return TRUE;
}

gboolean load_networks(struct global *global, struct ctrlproxy_config *cfg)
{
	GList *gl;
	g_assert(cfg);
	for (gl = cfg->networks; gl; gl = gl->next)
	{
		struct network_config *nc = gl->data;
		load_network(global, nc);
	}

	return TRUE;
}

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

struct network *find_network(struct global *global, const char *name)
{
	GList *gl;
	for (gl = global->networks; gl; gl = gl->next) {
		struct network *n = gl->data;
		if (n->name && !g_strcasecmp(n->name, name)) return n;
	}

	return NULL;
}

struct network *find_network_by_hostname(struct global *global, const char *hostname, guint16 port, gboolean create)
{
	GList *gl;
	struct network *n;
	char *portname = g_strdup_printf("%d", port);
	g_assert(portname);
	g_assert(hostname);
	
	for (gl = global->networks; gl; gl = gl->next) {
		GList *sv;
		n = gl->data;
		g_assert(n);

		if (n->name && !g_strcasecmp(n->name, hostname)) {
			g_free(portname);
			return n;
		}
		
		g_assert(n->config);
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
		nc = network_config_init(global->config);

		nc->name = g_strdup(hostname);
		nc->type = NETWORK_TCP;
		s->host = g_strdup(hostname);
		s->port = portname;

		nc->type_settings.tcp_servers = g_list_append(nc->type_settings.tcp_servers, s);

		return load_network(global, nc);
	}

	return NULL;
}

void fini_networks(struct global *global)
{
	GList *gl;
	while((gl = global->networks)) {
		struct network *n = (struct network *)gl->data;
		disconnect_network(n);
		unload_network(n);
	}
}

void network_select_next_server(struct network *n)
{
	g_assert(n);
	g_assert(n->config);

	if (n->config->type != NETWORK_TCP) 
		return;

	log_network(NULL, LOG_INFO, n, "Trying next server");
	n->connection.data.tcp.current_server = network_get_next_tcp_server(n);
}
