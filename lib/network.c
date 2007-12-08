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

#include "internals.h"
#include "irc.h"
#include "ssl.h"

static GHashTable *virtual_network_ops = NULL;

static gboolean delayed_connect_server(struct network *s);
static gboolean connect_server(struct network *s);
static gboolean close_server(struct network *s);
static void reconnect(struct network *server);
static void clients_send_state(GList *clients, struct network_state *s);
static gboolean server_finish_connect(GIOChannel *ioc, GIOCondition cond, 
								  void *data);

struct new_network_notify_data {
	new_network_notify_fn fn;
	void *data;
};

void register_new_network_notify(struct global *global, new_network_notify_fn fn, void *userdata)
{
	struct new_network_notify_data *p = g_new0(struct new_network_notify_data, 1);
	p->fn = fn;
	p->data = userdata;
	global->new_network_notifiers = g_list_append(global->new_network_notifiers, p);
}

static void state_log_helper(enum log_level l, void *userdata, const char *msg)
{
	network_log(l, (const struct network *)userdata, "%s", msg);
}

static void server_send_login (struct network *s) 
{
	g_assert(s);

	s->connection.state = NETWORK_CONNECTION_STATE_LOGIN_SENT;

	network_log(LOG_TRACE, s, "Sending login details");

	s->state = network_state_init(s->config->nick, s->config->username, 
								  get_my_hostname());
	network_state_set_log_fn(s->state, s->log, s->userdata);
	s->state->userdata = s;
	s->state->log = state_log_helper;
	s->linestack = new_linestack(s);
	g_assert(s->linestack != NULL);

	if (s->config->type == NETWORK_TCP && 
	   s->connection.data.tcp.current_server->password) { 
		network_send_args(s, "PASS", 
					s->connection.data.tcp.current_server->password, NULL);
	} else if (s->config->password) {
		network_send_args(s, "PASS", s->config->password, NULL);
	}
	network_send_args(s, "NICK", s->config->nick, NULL);
	network_send_args(s, "USER", s->config->username, s->config->username, 
					  s->config->name, s->config->fullname, NULL);
}

/**
 * Change the character set used to communicate with the server.
 *
 * @param n network to change the character set for
 * @param name name of the character set to use
 * @return true if setting the charset worked
 */
gboolean network_set_charset(struct network *n, const char *name)
{
	GIConv tmp;

	if (name != NULL) {
		tmp = g_iconv_open(name, "UTF-8");

		if (tmp == (GIConv)-1) {
			network_log(LOG_WARNING, n, "Unable to find charset `%s'", name);
			return FALSE;
		}
	} else {
		tmp = (GIConv)-1;
	}
	
	if (n->connection.outgoing_iconv != (GIConv)-1)
		g_iconv_close(n->connection.outgoing_iconv);

	n->connection.outgoing_iconv = tmp;

	if (name != NULL) {
		tmp = g_iconv_open("UTF-8", name);
		if (tmp == (GIConv)-1) {
			network_log(LOG_WARNING, n, "Unable to find charset `%s'", name);
			return FALSE;
		}
	} else {
		tmp = (GIConv)-1;
	}

	if (n->connection.incoming_iconv != (GIConv)-1)
		g_iconv_close(n->connection.incoming_iconv);

	n->connection.incoming_iconv = tmp;

	if (n->config->type == NETWORK_VIRTUAL)
		return TRUE;

	return TRUE;
}

/**
 * Update the isupport settings for a local network based on the 
 * ISUPPORT info sent by a server.
 *
 * @param net_info Current information
 * @param remote_info Remote information
 * @return Whether updating went ok.
 */
static gboolean network_update_isupport(struct network_info *net_info,
										struct network_info *remote_info)
{
	if (remote_info->name != NULL) {
		g_free(net_info->name);
		net_info->name = g_strdup(remote_info->name);
	}

	if (remote_info->prefix != NULL) {
		g_free(net_info->prefix);
		net_info->prefix = g_strdup(remote_info->prefix);
	}

	if (remote_info->chantypes != NULL) {
		g_free(net_info->chantypes);
		net_info->chantypes = g_strdup(remote_info->chantypes);
	}

	if (remote_info->casemapping != CASEMAP_UNKNOWN) {
		net_info->casemapping = remote_info->casemapping;
	}

	if (remote_info->supported_user_modes != NULL) {
		g_free(net_info->supported_user_modes);
		net_info->supported_user_modes = g_strdup(remote_info->supported_user_modes);
	} 

	if (remote_info->supported_channel_modes != NULL) {
		g_free(net_info->supported_channel_modes);
		net_info->supported_channel_modes = g_strdup(remote_info->supported_channel_modes);
	}

	return TRUE;
}

/**
 * Process a line received from the server
 *
 * @param n Network the line is received at
 * @param l Line received
 * @return Whether the message was received ok
 */
static gboolean process_from_server(struct network *n, struct line *l)
{
	struct line *lc;
	GError *error = NULL;

	g_assert(n);
	g_assert(l);

	log_network_line(n, l, TRUE);

	/* Silently drop empty messages, as allowed by RFC */
	if (l->argc == 0) {
		return TRUE;
	}

	n->connection.last_line_recvd = time(NULL);

	if (n->state == NULL) {
		network_log(LOG_WARNING, n, 
					"Dropping message '%s' because network is disconnected.", 
					l->args[0]);
		return FALSE;
	}

	run_log_filter(n, lc = linedup(l), FROM_SERVER); free_line(lc);
	run_replication_filter(n, lc = linedup(l), FROM_SERVER); free_line(lc);

	g_assert(n->state);

	state_handle_data(n->state, l);

	g_assert(l->args[0]);

	if (!g_strcasecmp(l->args[0], "PING")){
		network_send_args(n, "PONG", l->args[1], NULL);
		return TRUE;
	} else if (!g_strcasecmp(l->args[0], "PONG")){
		return TRUE;
	} else if (!g_strcasecmp(l->args[0], "ERROR")) {
		network_log(LOG_ERROR, n, "error: %s", l->args[1]);
	} else if (!g_strcasecmp(l->args[0], "433") && 
			  n->connection.state == NETWORK_CONNECTION_STATE_LOGIN_SENT){
		char *tmp = g_strdup_printf("%s_", l->args[2]);
		network_send_args(n, "NICK", tmp, NULL);
		network_log(LOG_WARNING, n, "%s was already in use, trying %s", 
					l->args[2], tmp);
		g_free(tmp);
	} else if (atoi(l->args[0]) == RPL_ENDOFMOTD ||
			  atoi(l->args[0]) == ERR_NOMOTD) {
		GList *gl;
		n->connection.state = NETWORK_CONNECTION_STATE_MOTD_RECVD;

		network_set_charset(n, get_charset(&n->info));

		if (error != NULL)
			network_log(LOG_WARNING, n, "Error setting charset %s: %s", 
				get_charset(&n->info), error->message);

		network_log(LOG_INFO, n, "Successfully logged in");

		network_update_isupport(&n->info, &n->state->info);

		nickserv_identify_me(n, n->state->me.nick);

		clients_send_state(n->clients, n->state);

		server_connected_hook_execute(n);

		network_send_args(n, "USERHOST", n->state->me.nick, NULL);

		/* Rejoin channels */
		for (gl = n->config->channels; gl; gl = gl->next) {
			struct channel_config *c = gl->data;

			if (c->autojoin) {
				network_send_args(n, "JOIN", c->name, c->key, NULL);
			} 
		}
	} 

	if ( n->connection.state == NETWORK_CONNECTION_STATE_MOTD_RECVD) {
		gboolean linestack_store = TRUE;
		if (atoi(l->args[0])) {
			linestack_store &= (!redirect_response(n, l));
		} else {
			if (n->clients == NULL) {
				if (!g_strcasecmp(l->args[0], "PRIVMSG") && l->argc > 2 && 
					l->args[2][0] == '\001' && 
					g_strncasecmp(l->args[2], "\001ACTION", 7) != 0) {
					ctcp_process_network_request(n, l);
				} else if (!g_strcasecmp(l->args[0], "NOTICE") && l->argc > 2 && 
					l->args[2][0] == '\001') {
					ctcp_process_network_reply(n, l);
				}
			} else if (run_server_filter(n, l, FROM_SERVER)) {
				if (!strcmp(l->args[0], "PRIVMSG") && 
					n->global->config->report_time == REPORT_TIME_ALWAYS)
					l = line_prefix_time(l, time(NULL)+n->global->config->report_time_offset);

				clients_send(n->clients, l, NULL);
			}
		} 

		if (linestack_store)
			linestack_insert_line(n->linestack, l, FROM_SERVER, n->state);
	} 

	return TRUE;
}

static void network_report_disconnect(struct network *n, const char *fmt, ...)
{
	va_list ap;
	char *tmp;
	va_start(ap, fmt);
	tmp = g_strdup_vprintf(fmt, ap);
	va_end(ap);

	g_free(n->connection.data.tcp.last_disconnect_reason);
	n->connection.data.tcp.last_disconnect_reason = tmp;

	network_log(LOG_WARNING, n, "%s", tmp);
}

static gboolean handle_server_receive (GIOChannel *c, GIOCondition cond, void *_server)
{
	struct network *server = (struct network *)_server;
	struct line *l;
	gboolean ret;

	g_assert(c != NULL);
	g_assert(server != NULL);

	if (cond & G_IO_HUP) {
		network_report_disconnect(server, "Hangup from server, scheduling reconnect");
		reconnect(server);
		return FALSE;
	}

	if (cond & G_IO_ERR) {
		network_report_disconnect(server, 
								  "Error from server: %s, scheduling reconnect", g_io_channel_unix_get_sock_error(c));
		reconnect(server);
		return FALSE;
	}

	if (cond & G_IO_IN) {
		GError *err = NULL;
		GIOStatus status;
		
		while ((status = irc_recv_line(c, server->connection.incoming_iconv, 
									   &err, &l)) == G_IO_STATUS_NORMAL) 
		{
			g_assert(l != NULL);

			ret = process_from_server(server, l);

			free_line(l);

			if (!ret)
				return FALSE;
		}

		switch (status) {
		case G_IO_STATUS_AGAIN:
			return TRUE;
		case G_IO_STATUS_EOF:
			if (server->connection.state != NETWORK_CONNECTION_STATE_NOT_CONNECTED) 
				reconnect(server);
			return FALSE;
		case G_IO_STATUS_ERROR:
			g_assert(err != NULL);
			network_log(LOG_WARNING, server, 
						"Error \"%s\" reading from server", err->message);
			if (l != NULL) {
				ret = process_from_server(server, l);

				free_line(l);

				return ret;
			}

			return TRUE;
		default: abort();
		}

		return TRUE;
	}

	return TRUE;
}

static struct tcp_server_config *network_get_next_tcp_server(struct network *n)
{
	GList *cur;
	
	g_assert(n);
	g_assert(n->config);
	cur = g_list_find(n->config->type_settings.tcp_servers, n->connection.data.tcp.current_server);

	/* Get next available server */
	if (cur != NULL && cur->next != NULL) 
		cur = cur->next; 
	else 
		cur = n->config->type_settings.tcp_servers;

	if (cur != NULL) 
		return cur->data; 

	return NULL;
}

static gboolean antiflood_allow_line(struct network *s)
{
	/* FIXME: Implement antiflood, use s->config->queue_speed */
	return TRUE;
}

/**
 * Actually send items from the server queue to the server.
 * 
 * @param ch IO Channel to use
 * @param cond IO Condition
 * @param user_data Network pointer
 * @return Whether the queue still contains items
 */
static gboolean server_send_queue(GIOChannel *ch, GIOCondition cond, 
								  gpointer user_data)
{
	struct network *s = user_data;
	GError *error = NULL;
	GIOStatus status;

	while (!g_queue_is_empty(s->connection.pending_lines)) {
		struct line *l = g_queue_peek_head(s->connection.pending_lines);

		/* Check if antiflood allows us to send a line */
		if (!antiflood_allow_line(s)) 
			return TRUE;

		status = irc_send_line(s->connection.outgoing, s->connection.outgoing_iconv, l, &error);

		if (status == G_IO_STATUS_AGAIN)
			return TRUE;

		g_queue_pop_head(s->connection.pending_lines);

		if (status != G_IO_STATUS_NORMAL) {
			network_log(LOG_WARNING, s, "Error sending line '%s': %s",
	           l->args[0], error != NULL?error->message:"ERROR");
			free_line(l);

			return FALSE;
		}
		s->connection.last_line_sent = time(NULL);

		free_line(l);
	}

	s->connection.outgoing_id = 0;
	return FALSE;
}

static gboolean network_send_line_direct(struct network *s, struct client *c, 
										 const struct line *ol)
{
	struct line nl, *l;

	g_assert(s->config != NULL);

	g_assert(s->config->type == NETWORK_TCP ||
		 s->config->type == NETWORK_PROGRAM ||
		 s->config->type == NETWORK_IOCHANNEL ||
		 s->config->type == NETWORK_VIRTUAL);

	l = &nl;
	memcpy(l, ol, sizeof(struct line));
	nl.origin = NULL;

	/* origin lines should never be sent to the server */
	g_assert(l->origin == NULL);

	if (s->config->type == NETWORK_VIRTUAL) {
		if (s->connection.data.virtual.ops == NULL) 
			return FALSE;
		return s->connection.data.virtual.ops->to_server(s, c, l);
	} else if (s->connection.outgoing_id == 0) {
		GError *error = NULL;

		GIOStatus status = irc_send_line(s->connection.outgoing, s->connection.outgoing_iconv, l, &error);

		if (status == G_IO_STATUS_AGAIN) {
			g_queue_push_tail(s->connection.pending_lines, linedup(l));
			s->connection.outgoing_id = g_io_add_watch(s->connection.outgoing, G_IO_OUT, server_send_queue, s);
		} else if (status != G_IO_STATUS_NORMAL) {
			network_log(LOG_WARNING, s, "Error sending line '%s': %s",
	           l->args[0], error != NULL?error->message:"ERROR");
			return FALSE;
		}

		s->connection.last_line_sent = time(NULL);
		return TRUE;
	} else {
		g_queue_push_tail(s->connection.pending_lines, linedup(l));
	}

	return TRUE;
}

/**
 * Send a line to the network.
 * @param s Network to send to.
 * @param c Client the line was sent by originally.
 * @param ol Line to send to the network
 * @param is_private Whether the line should not be broadcast to other clients
 */
gboolean network_send_line(struct network *s, struct client *c, 
						   const struct line *ol, gboolean is_private)
{
	struct line l;
	char *tmp = NULL;
	struct line *lc;

	g_assert(ol);
	g_assert(s);
	l = *ol;

	if (l.origin == NULL && s->state != NULL) {
		tmp = l.origin = g_strdup(s->state->me.hostmask);
	}

	if (l.origin != NULL) {
		if (!run_server_filter(s, &l, TO_SERVER)) {
			g_free(tmp);
			return TRUE;
		}

		run_log_filter(s, lc = linedup(&l), TO_SERVER); free_line(lc);
		run_replication_filter(s, lc = linedup(&l), TO_SERVER); free_line(lc);
		linestack_insert_line(s->linestack, ol, TO_SERVER, s->state);
	}

	g_assert(l.args[0] != NULL);

	/* Also write this message to all other clients currently connected */
	if (!is_private && 
	   (!g_strcasecmp(l.args[0], "PRIVMSG") || 
		!g_strcasecmp(l.args[0], "NOTICE"))) {
		g_assert(l.origin);
		if (s->global->config->report_time == REPORT_TIME_ALWAYS)
			line_prefix_time(&l, time(NULL)+s->global->config->report_time_offset);

		clients_send(s->clients, &l, c);
	}

	g_free(tmp);

	log_network_line(s, ol, FALSE);

	redirect_record(s, c, ol);

	return network_send_line_direct(s, c, ol);
}

/**
 * Indicate that a response is received by a virtual network.
 *
 * @param n Network to receive data
 * @param num Number of the response to receive
 */
gboolean virtual_network_recv_response(struct network *n, int num, ...) 
{
	va_list ap;
	struct line *l;
	gboolean ret;

	g_assert(n);
	g_assert(n->config->type == NETWORK_VIRTUAL);

	va_start(ap, num);
	l = virc_parse_line(n->info.name, ap);
	va_end(ap);

	l->args = g_realloc(l->args, sizeof(char *) * (l->argc+4));
	memmove(&l->args[2], &l->args[0], l->argc * sizeof(char *));

	l->args[0] = g_strdup_printf("%03d", num);

	if (n->state != NULL && n->state->me.nick != NULL) 
		l->args[1] = g_strdup(n->state->me.nick);
	else 
		l->args[1] = g_strdup("*");

	l->argc+=2;
	l->args[l->argc] = NULL;

	ret = virtual_network_recv_line(n, l);

	free_line(l);

	return ret;
}

/**
 * Indicate that a line is received by a virtual network.
 *
 * @param s Network to send to.
 * @param l Line to receive.
 */
gboolean virtual_network_recv_line(struct network *s, struct line *l)
{
	g_assert(s != NULL);
	g_assert(l != NULL);

	if (l->origin == NULL) 
		l->origin = g_strdup(get_my_hostname());

	return process_from_server(s, l);
}

/**
 * Indicate that a line has been received.
 *
 * @param s Network to use.
 * @param origin Origin to make the data originate from
 */
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

/**
 * Send a new line to the network.
 *
 * @param s Network
 * @param ... Arguments terminated by NULL
 */
gboolean network_send_args(struct network *s, ...)
{
	va_list ap;
	struct line *l;
	gboolean ret;

	g_assert(s);

	va_start(ap, s);
	l = virc_parse_line(NULL, ap);
	va_end(ap);

	ret = network_send_line(s, NULL, l, TRUE);

	free_line(l);

	return ret;
}

static gboolean bindsock(struct network *s,
						 int sock, struct addrinfo *res, 
						 const char *address,
						 const char *service)
{
	struct addrinfo hints_bind;
	int error;
	struct addrinfo *res_bind, *addrinfo_bind;

	memset(&hints_bind, 0, sizeof(hints_bind));
	hints_bind.ai_family = res->ai_family;
	hints_bind.ai_flags = AI_ADDRCONFIG;
	hints_bind.ai_socktype = res->ai_socktype;
	hints_bind.ai_protocol = res->ai_protocol;

	error = getaddrinfo(address, service, &hints_bind, &addrinfo_bind);
	if (error) {
		network_log(LOG_ERROR, s, 
					"Unable to lookup %s:%s %s", address, service, 
					gai_strerror(error));
		return FALSE;
	} 

	for (res_bind = addrinfo_bind; 
		 res_bind; res_bind = res_bind->ai_next) {
		if (bind(sock, res_bind->ai_addr, res_bind->ai_addrlen) < 0) {
			network_log(LOG_ERROR, s, "Unable to bind to %s:%s %s", 
						address, service, strerror(errno));
		} else 
			break;
	}
	freeaddrinfo(addrinfo_bind);

	return (res_bind != NULL);
}

/**
 * Ping the network.
 *
 * @param server network to ping
 * @param ping_source GSource id of the ping event
 */
static void ping_server(struct network *server, gboolean ping_source)
{
	gint silent_time = time(NULL) - server->connection.last_line_recvd;
	if (silent_time > MAX_SILENT_TIME) {
		network_report_disconnect(server, "Ping timeout (%d seconds)", 
								  silent_time);
		reconnect(server);
	} else if (silent_time > MIN_SILENT_TIME) {
		network_send_args(server, "PING", "ctrlproxy", NULL);
	}
}

static gboolean connect_current_tcp_server(struct network *s) 
{
	struct addrinfo *res;
	int sock = -1;
	struct tcp_server_config *cs;
	GIOChannel *ioc = NULL;
	struct addrinfo hints;
	struct addrinfo *addrinfo = NULL;
	int error;
	socklen_t size;
	gboolean connect_finished = TRUE;

	g_assert(s != NULL);

	if (!s->connection.data.tcp.current_server) {
		s->connection.data.tcp.current_server = network_get_next_tcp_server(s);
	}

	network_log(LOG_TRACE, s, "connect_current_tcp_server");
	
	cs = s->connection.data.tcp.current_server;
	if (cs == NULL) {
		s->config->autoconnect = FALSE;
		network_log(LOG_WARNING, s, "No servers listed, not connecting");
		return FALSE;
	}

	network_log(LOG_INFO, s, "Connecting with %s:%s", cs->host, cs->port);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_ADDRCONFIG;

	/* Lookup */
	error = getaddrinfo(cs->host, cs->port, &hints, &addrinfo);
	if (error) {
		network_log(LOG_ERROR, s, "Unable to lookup %s:%s %s", 
					cs->host, cs->port, gai_strerror(error));
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

		if (cs->bind_address || cs->bind_port)
			bindsock(s, sock, res, cs->bind_address, cs->bind_port);

		ioc = g_io_channel_unix_new(sock);
		g_io_channel_set_flags(ioc, G_IO_FLAG_NONBLOCK, NULL);

		if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
			if (errno == EINPROGRESS) {
				connect_finished = FALSE;
				break;
			}
			g_io_channel_unref(ioc);
			ioc = NULL;
			continue;
		}

		break; 
	}

	size = sizeof(struct sockaddr_storage);
	g_assert(s->connection.data.tcp.local_name == NULL);
	g_assert(s->connection.data.tcp.remote_name == NULL);
	s->connection.data.tcp.remote_name = g_memdup(res->ai_addr, 
												  res->ai_addrlen);
	s->connection.data.tcp.local_name = g_malloc(size);
	s->connection.data.tcp.namelen = getsockname(sock, s->connection.data.tcp.local_name, &size);

	freeaddrinfo(addrinfo);

	if (!ioc) {
		network_log(LOG_ERROR, s, "Unable to connect: %s", strerror(errno));
		return FALSE;
	}

	g_io_channel_set_close_on_unref(ioc, TRUE);

	s->connection.state = NETWORK_CONNECTION_STATE_CONNECTING;

	if (!connect_finished) {
		s->connection.outgoing_id = g_io_add_watch(ioc, 
												   G_IO_OUT|G_IO_ERR|G_IO_HUP, 
												   server_finish_connect, s);
	} else {
		server_finish_connect(ioc, G_IO_OUT, s);
	}

	g_io_channel_unref(ioc);

	return TRUE;
}

static void reconnect(struct network *server)
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
		server->config->type == NETWORK_IOCHANNEL ||
		server->config->type == NETWORK_PROGRAM) {
		server->connection.state = NETWORK_CONNECTION_STATE_RECONNECT_PENDING;
		network_log(LOG_INFO, server, "Reconnecting in %d seconds", 
					server->config->reconnect_interval);
		server->reconnect_id = g_timeout_add(1000 * 
								server->config->reconnect_interval, 
								(GSourceFunc) delayed_connect_server, server);
	} else {
		connect_server(server);	
	}
}

static void clients_send_state(GList *clients, struct network_state *s)
{
	GList *gl;

	for (gl = clients; gl; gl = gl->next) {
		struct client *c = gl->data;
		client_send_state(c, s);
	}
}

static void clients_invalidate_state(GList *clients, struct network_state *s)
{
	GList *gl;

	/* Leave channels */
	for (gl = s->channels; gl; gl = gl->next) {
		struct channel_state *ch = gl->data;

		clients_send_args_ex(clients, s->me.hostmask, "PART", ch->name, 
							 "Network disconnected", NULL);
	}

	/* private queries quit */
	for (gl = s->nicks; gl; gl = gl->next) {
		struct network_nick *gn = gl->data;

		if (!gn->query || gn == &s->me) continue;

		clients_send_args_ex(clients, gn->hostmask, "QUIT", "Network disconnected", NULL);
	}
}

static gboolean close_server(struct network *n) 
{
	g_assert(n);

	if (n->connection.state == NETWORK_CONNECTION_STATE_RECONNECT_PENDING) {
		g_source_remove(n->reconnect_id);
		n->reconnect_id = 0;
		n->connection.state = NETWORK_CONNECTION_STATE_NOT_CONNECTED;
	}

	if (n->connection.state == NETWORK_CONNECTION_STATE_NOT_CONNECTED) {
		return FALSE;
	} 

	network_send_args(n, "QUIT", NULL);

	if (n->connection.state == NETWORK_CONNECTION_STATE_MOTD_RECVD) {
		server_disconnected_hook_execute(n);
		clients_invalidate_state(n->clients, n->state);
		network_update_config(n->state, n->config);
	}

	if (n->state) {
		free_linestack_context(n->linestack);
		n->linestack = NULL;
		free_network_state(n->state); 
		n->state = NULL;
	}

	g_assert(n->config);

	switch (n->config->type) {
	case NETWORK_TCP: 
	case NETWORK_PROGRAM: 
	case NETWORK_IOCHANNEL:
		if (n->connection.incoming_id > 0) {
			g_source_remove(n->connection.incoming_id); 
			n->connection.incoming_id = 0;
		}
		if (n->connection.outgoing_id > 0) {
			g_source_remove(n->connection.outgoing_id); 
			n->connection.outgoing_id = 0;
		}
		if (n->connection.data.tcp.ping_id > 0) {
			g_source_remove(n->connection.data.tcp.ping_id);
			n->connection.data.tcp.ping_id = 0;
		}
		g_free(n->connection.data.tcp.local_name);
		g_free(n->connection.data.tcp.remote_name);
		n->connection.data.tcp.local_name = NULL;
		n->connection.data.tcp.remote_name = NULL;
		break;
	case NETWORK_VIRTUAL:
		if (n->connection.data.virtual.ops && 
			n->connection.data.virtual.ops->fini) {
			n->connection.data.virtual.ops->fini(n);
		}
		break;
		default: g_assert(0);
	}

	n->connection.state = NETWORK_CONNECTION_STATE_NOT_CONNECTED;
	redirect_clear(n);

	return TRUE;
}

void clients_send_args_ex(GList *clients, const char *hostmask, ...)
{
	struct line *l;
	va_list ap;

	va_start(ap, hostmask);
	l = virc_parse_line(hostmask, ap);
	va_end(ap);

	clients_send(clients, l, NULL);

	free_line(l); l = NULL;
}

/**
 * Send a line to a list of clients.
 *
 * @param clients List of clients to send to
 * @param l Line to send
 * @param exception Client to which nothing should be sent. Can be NULL.
 */
void clients_send(GList *clients, struct line *l, 
				  const struct client *exception) 
{
	GList *g;

	for (g = clients; g; g = g->next) {
		struct client *c = (struct client *)g->data;
		if (c != exception) {
			if (run_client_filter(c, l, FROM_SERVER)) { 
				client_send_line(c, l);
			}
		}
	}
}

static pid_t piped_child(char* const command[], int *f_in)
{
	pid_t pid;
	int sock[2];

	if (socketpair(PF_UNIX, SOCK_STREAM, AF_LOCAL, sock) == -1) {
		log_global(LOG_ERROR, "socketpair: %s", strerror(errno));
		return -1;
	}

	*f_in = sock[0];

	fcntl(sock[0], F_SETFL, O_NONBLOCK);

	pid = fork();

	if (pid == -1) {
		log_global(LOG_ERROR, "fork: %s", strerror(errno));
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

static gboolean server_finish_connect(GIOChannel *ioc, GIOCondition cond, 
								  void *data)
{
	struct network *s = data;
	struct tcp_server_config *cs;

	if (cond & G_IO_ERR) {
		network_report_disconnect(s, "Error connecting: %s", 
								  g_io_channel_unix_get_sock_error(ioc));
		reconnect(s);
		return FALSE;
	}

	if (cond & G_IO_OUT) {
		s->connection.state = NETWORK_CONNECTION_STATE_CONNECTED;

		cs = s->connection.data.tcp.current_server;
		if (cs->ssl) {
#ifdef HAVE_GNUTLS
			g_io_channel_set_close_on_unref(ioc, TRUE);
			g_io_channel_set_flags(ioc, G_IO_FLAG_NONBLOCK, NULL);

			ioc = ssl_wrap_iochannel (ioc, SSL_TYPE_CLIENT, 
									 s->connection.data.tcp.current_server->host,
									 s->ssl_credentials
									 );
			g_assert(ioc != NULL);
#else
			network_log(LOG_WARNING, s, "SSL enabled for %s:%s, but no SSL support loaded", cs->host, cs->port);
#endif
		}

		if (!ioc) {
			network_report_disconnect(s, "Couldn't connect via server %s:%s", cs->host, cs->port);
			reconnect(s);
			return FALSE;
		}

		s->connection.outgoing_id = 0; /* Otherwise data will be queued */
		network_set_iochannel(s, ioc);

		s->connection.last_line_recvd = time(NULL);
		s->connection.data.tcp.ping_id = g_timeout_add(5000, 
									   (GSourceFunc) ping_server, s);

		return FALSE;
	}

	if (cond & G_IO_HUP) {
		network_report_disconnect(s, "Server closed connection");
		reconnect(s);
		return FALSE;
	}

	return FALSE;
}

/**
 * Change the IO channel used to communicate with a network.
 * @param s Network to set the IO channel for.
 * @param ioc IO channel to use
 */
gboolean network_set_iochannel(struct network *s, GIOChannel *ioc)
{
	GError *error = NULL;
	g_assert(s->config->type != NETWORK_VIRTUAL);
	if (g_io_channel_set_encoding(ioc, NULL, &error) != G_IO_STATUS_NORMAL) {
		network_log(LOG_ERROR, s, "Unable to change encoding: %s", 
					error?error->message:"unknown");
		return FALSE;
	}
	g_io_channel_set_close_on_unref(ioc, TRUE);
	if (g_io_channel_set_flags(ioc, G_IO_FLAG_NONBLOCK, &error) != G_IO_STATUS_NORMAL) {
		network_log(LOG_ERROR, s, "Unable to change flags: %s", 
					error?error->message:"unknown");
		return FALSE;
	}

	s->connection.outgoing = ioc;

	s->connection.incoming_id = g_io_add_watch(s->connection.outgoing, 
								G_IO_IN | G_IO_HUP | G_IO_ERR, 
								handle_server_receive, s);
	handle_server_receive(s->connection.outgoing, 
				  g_io_channel_get_buffer_condition(s->connection.outgoing), 
				  s);

	server_send_login(s);

	return TRUE;
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

	network_set_iochannel(s, g_io_channel_unix_new(sock));

	g_io_channel_unref(s->connection.outgoing);

	if (s->info.name == NULL) {
		if (strchr(s->config->type_settings.program_location, '/')) {
			s->info.name = g_strdup(strrchr(s->config->type_settings.program_location, '/')+1);
		} else {
			s->info.name = g_strdup(s->config->type_settings.program_location);
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
		s->connection.data.virtual.ops = g_hash_table_lookup(
								virtual_network_ops, 
								s->config->type_settings.virtual_type);
		if (!s->connection.data.virtual.ops) 
			return FALSE;

		s->state = network_state_init(s->config->nick, s->config->username, 
									  get_my_hostname());
		s->state->userdata = s;
		s->state->log = state_log_helper;
		s->linestack = new_linestack(s);
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

/**
 * Load a network from a configuration file specification.
 *
 * @param global Global context to use.
 * @param sc Network configuration to load form
 * @return A new network instance, already added to the global context.
 */
struct network *load_network(struct global *global, struct network_config *sc)
{
	struct network *s;
	GList *gl;

	g_assert(sc);

	if (global != NULL) {
		/* Don't connect to the same network twice */
		s = find_network(global, sc->name);
		if (s) 
			return s;
	}

	s = g_new0(struct network, 1);
	s->references = 1;
	s->config = sc;
	network_info_init(&s->info);
	s->info.name = g_strdup(s->config->name);
	s->info.ircd = g_strdup("ctrlproxy");
	s->connection.pending_lines = g_queue_new();
	s->global = global;
	s->info.forced_nick_changes = TRUE; /* Forced nick changes are done by ctrlproxy */
	s->connection.outgoing_iconv = s->connection.incoming_iconv = (GIConv)-1;

	if (global != NULL) {
		g_free(s->info.charset);
		if (s->global->config->client_charset != NULL) {
			s->info.charset = g_strdup(s->global->config->client_charset);
		}

		global->networks = g_list_append(global->networks, s);

		for (gl = global->new_network_notifiers; gl; gl = gl->next) {
			struct new_network_notify_data *p = gl->data;

			p->fn(s, p->data);
		}
	}

#ifdef HAVE_GNUTLS
	s->ssl_credentials = ssl_get_client_credentials(NULL);
#endif

	return s;
}

/**
 * Connect to a network, returns TRUE if connection was successful 
 * (or startup of connection was successful) 
 *
 * @param s Network to connect to
 */
gboolean connect_network(struct network *s) 
{
	g_assert(s);
	g_assert(s->connection.state == NETWORK_CONNECTION_STATE_NOT_CONNECTED ||
			 s->connection.state == NETWORK_CONNECTION_STATE_RECONNECT_PENDING);

	return connect_server(s);
}

static void free_pending_line(void *_line, void *userdata)
{
	free_line((struct line *)_line);
}

static void free_network(struct network *s)
{
	g_queue_foreach(s->connection.pending_lines, free_pending_line, NULL);
	g_queue_free(s->connection.pending_lines);

	free_network_info(&s->info);
	if (s->config->type == NETWORK_TCP)
		g_free(s->connection.data.tcp.last_disconnect_reason);

#ifdef HAVE_GNUTLS
	ssl_free_client_credentials(s->ssl_credentials);
#endif

	g_iconv_close(s->connection.incoming_iconv);
	g_iconv_close(s->connection.outgoing_iconv);

	g_free(s);
}

/** 
 * Unload a network from a global context.
 *
 * @param s Network to unload.
 */
void unload_network(struct network *s)
{
	GList *l;
	
	g_assert(s);
	l = s->clients;

	while(l) {
		struct client *c = l->data;
		l = l->next;
		disconnect_client(c, "Server exiting");
	}

	if (s->global != NULL) {
		s->global->networks = g_list_remove(s->global->networks, s);
	}

	network_unref(s);
}

/** 
 * Disconnect from a network. The network will still be kept in memory, 
 * but all socket connections associated with it will be dropped.
 *
 * @param s Network to disconnect from
 * @return Whether disconnecting succeeded.
 */
gboolean disconnect_network(struct network *s)
{
	g_assert(s);
	if (s->connection.state == NETWORK_CONNECTION_STATE_NOT_CONNECTED) {
		return FALSE;
	}

	network_log(LOG_INFO, s, "Disconnecting");
	return close_server(s);
}

/**
 * Register a new virtual network type.
 *
 * @param ops Callback functions for the virtual network type.
 */
void register_virtual_network(struct virtual_network_ops *ops)
{
	if (virtual_network_ops == NULL)
		virtual_network_ops = g_hash_table_new(g_str_hash, g_str_equal);
	g_assert(ops);
	g_hash_table_insert(virtual_network_ops, ops->name, ops);
}

/**
 * Autoconnect to all the networks in a global context.
 *
 * @param Global global context
 * @return TRUE
 */
gboolean autoconnect_networks(struct global *global)
{
	GList *gl;
	for (gl = global->networks; gl; gl = gl->next)
	{
		struct network *n = gl->data;
		g_assert(n);
		g_assert(n->config);
		if (n->config->autoconnect)
			connect_network(n);
	}

	return TRUE;
}

/**
 * Load all the networks in a configuration file.
 *
 * @param global Global context to load networks into.
 * @param cfg Configuration to read from
 * @return TRUE
 */
gboolean load_networks(struct global *global, struct ctrlproxy_config *cfg, 
					   network_log_fn fn)
{
	GList *gl;
	g_assert(cfg);
	for (gl = cfg->networks; gl; gl = gl->next)
	{
		struct network_config *nc = gl->data;
		struct network *n;
		n = load_network(global, nc);
		if (n != NULL)
			network_set_log_fn(n, fn, n);
	}

	return TRUE;
}

/**
 * Find a network by name.
 *
 * @param global Global context to search in.
 * @param name Name of the network to search for.
 * @return first network found or NULL
 */
struct network *find_network(struct global *global, const char *name)
{
	GList *gl;
	for (gl = global->networks; gl; gl = gl->next) {
		struct network *n = gl->data;
		if (n->info.name && !g_strcasecmp(n->info.name, name)) 
			return n;
	}

	return NULL;
}

/**
 * Find a network by host name and port or name.
 * 
 * @param global Context to search in
 * @param hostname Hostname to search for
 * @param port Port to search from.
 * @param create Whether to create the network if it wasn't found.
 * @return the network found or created or NULL
 */
struct network *find_network_by_hostname(struct global *global, 
										 const char *hostname, guint16 port, 
										 gboolean create)
{
	GList *gl;
	char *portname = g_strdup_printf("%d", port);
	g_assert(portname != NULL);
	g_assert(hostname != NULL);
	
	for (gl = global->networks; gl; gl = gl->next) {
		GList *sv;
		struct network *n = gl->data;
		g_assert(n);

		if (n->info.name && !g_strcasecmp(n->info.name, hostname)) {
			g_free(portname);
			return n;
		}

		g_assert(n->config);
		if (n->config->type == NETWORK_TCP) 
		{
			for (sv = n->config->type_settings.tcp_servers; sv; sv = sv->next)
			{
				struct tcp_server_config *server = sv->data;

				if (!g_strcasecmp(server->host, hostname) && 
					!g_strcasecmp(server->port, portname)) {
					g_free(portname);
					return n;
				}
			} 
		}

		if (n->info.name && !g_strcasecmp(n->info.name, hostname)) {
			g_free(portname);
			return n;
		}
	}

	/* Create a new server */
	if (create)
	{
		struct tcp_server_config *s = g_new0(struct tcp_server_config, 1);
		struct network_config *nc;
		struct network *n;
		nc = network_config_init(global->config);

		nc->name = g_strdup(hostname);
		nc->type = NETWORK_TCP;
		s->host = g_strdup(hostname);
		s->port = portname;

		nc->type_settings.tcp_servers = g_list_append(nc->type_settings.tcp_servers, s);

		n = load_network(global, nc);
		network_set_log_fn(n, (network_log_fn)handle_network_log, n);
		return n;
	}

	g_free(portname);

	return NULL;
}

/**
 * Disconnect from and unload all networks.
 *
 * @param global Global context
 */
void fini_networks(struct global *global)
{
	GList *gl;
	while((gl = global->networks)) {
		struct network *n = (struct network *)gl->data;
		disconnect_network(n);
		unload_network(n);
	}

	if (virtual_network_ops != NULL)
		g_hash_table_destroy(virtual_network_ops);
	virtual_network_ops = NULL;
}

/**
 * Switch to the next server listed for a network.
 *
 * @param n Network
 */
void network_select_next_server(struct network *n)
{
	g_assert(n);
	g_assert(n->config);

	if (n->config->type != NETWORK_TCP) 
		return;

	network_log(LOG_INFO, n, "Trying next server");
	n->connection.data.tcp.current_server = network_get_next_tcp_server(n);
}

/**
 * Generate 005-response string to send to client connected to a network.
 *
 * @param n Network to generate for
 * @return An 005 string, newly allocated
 */
char *network_generate_feature_string(struct network *n)
{
	g_assert(n);

	return network_info_string(&n->info);
}

/**
 * Increase the reference count for a network
 */
struct network *network_ref(struct network *n)
{
	if (n != NULL)
		n->references++;
	return n;
}

void network_unref(struct network *n)
{
	if (n == NULL)
		return;
	n->references--;
	if (n->references == 0) 
		free_network(n);
}

void network_log(enum log_level l, const struct network *s, 
				 const char *fmt, ...)
{
	char *ret;
	va_list ap;

	if (s->log == NULL)
		return;

	g_assert(s);
	g_assert(fmt);

	va_start(ap, fmt);
	ret = g_strdup_vprintf(fmt, ap);
	va_end(ap);

	s->log(l, s->userdata, ret);

	g_free(ret);
}

void network_set_log_fn(struct network *s, 
						network_log_fn fn,
						void *userdata)
{
	s->log = fn;
	s->userdata = userdata;

	if (s->state != NULL)
		network_state_set_log_fn(s->state, fn, userdata);
}
