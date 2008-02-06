/* 
	ctrlproxy: A modular IRC proxy
	(c) 2005-2006 Jelmer Vernooij <jelmer@nl.linux.org>
	
	Manual listen on ports

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
#include "lib/listener.h"
#include "ssl.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#include <netdb.h>
#include "socks.h"

struct listener_iochannel {
	char address[NI_MAXHOST];
	char port[NI_MAXSERV];
	gint watch_id;
};

static gboolean handle_client_detect(GIOChannel *ioc, 
									 struct pending_client *cl);
static gboolean handle_client_socks_data(GIOChannel *ioc, 
										 struct pending_client *cl);


static gboolean kill_pending_client(struct pending_client *pc)
{
	pc->listener->pending = g_list_remove(pc->listener->pending, pc);

	g_source_remove(pc->watch_id);

	g_free(pc->clientname);

	g_free(pc);

	return TRUE;
}



gboolean stop_listener(struct irc_listener *l)
{
	while (l->incoming != NULL) {
		struct listener_iochannel *lio = l->incoming->data;

		g_source_remove(lio->watch_id);
		
		listener_log(LOG_INFO, l, "Stopped listening at %s:%s", lio->address, 
					 lio->port);
		g_free(lio);

		l->incoming = g_list_remove(l->incoming, lio);
	}

	return TRUE;
}

static gboolean handle_client_detect(GIOChannel *ioc, struct pending_client *pc)
{
	GIOStatus status;
	gsize read;
	gchar header[2];

	status = g_io_channel_read_chars(ioc, header, 1, &read, NULL);

	if (status != G_IO_STATUS_NORMAL) {
		return FALSE;
	}

	if (header[0] == SOCKS_VERSION) {
		pc->socks.state = SOCKS_STATE_NEW;
		return TRUE;
	} else {
		struct irc_line *l = NULL;
		gchar *raw = NULL, *cvrt = NULL;
		gchar *complete;
		GIOStatus status;
		gboolean ret;
		GError *error = NULL;
		gsize in_len;

		pc->socks.state = SOCKS_UNUSED;

		g_assert(ioc != NULL);

		status = g_io_channel_read_line(ioc, &raw, &in_len, NULL, &error);
		if (status != G_IO_STATUS_NORMAL) {
			g_free(raw);
			return status;
		}

		complete = g_malloc(in_len+2);
		complete[0] = header[0];
		memcpy(complete+1, raw, in_len);
		complete[in_len+1] = '\0';
		g_free(raw);

		if (pc->listener->iconv == (GIConv)-1) {
			cvrt = complete;
		} else {
			cvrt = g_convert_with_iconv(complete, -1, pc->listener->iconv, NULL, NULL, &error);
			if (cvrt == NULL) {
				cvrt = complete;
				status = G_IO_STATUS_ERROR;
			} else {
				g_free(complete);
			}
		}

		l = irc_parse_line(cvrt);

		ret = pc->listener->handle_client_line(ioc, pc, l);

		free_line(l);

		g_free(cvrt);

		return ret;
	}
}



static gboolean handle_client_receive(GIOChannel *c, GIOCondition condition, gpointer data) 
{
	struct pending_client *pc = data;

	g_assert(c != NULL);

	if (condition & G_IO_IN) {
		if (pc->socks.state == SOCKS_UNKNOWN) {
			if (!handle_client_detect(c, pc)) {
				kill_pending_client(pc);
				return FALSE;
			}
		}

		if (pc->socks.state == SOCKS_UNUSED) {
			GIOStatus status;
			GError *error = NULL;
			struct irc_line *l;

			while ((status = irc_recv_line(c, pc->listener->iconv, &error, &l)) == G_IO_STATUS_NORMAL) {
				gboolean ret;

				if (l == NULL)
					continue;

				ret = pc->listener->handle_client_line(c, pc, l);

				free_line(l);

				if (!ret) {
					kill_pending_client(pc);
					return FALSE;
				}
			}

			if (status != G_IO_STATUS_AGAIN) {
				kill_pending_client(pc);
				return FALSE;
			}
		} else {
			gboolean ret = handle_client_socks_data(c, pc);
			if (!ret)
				kill_pending_client(pc);
			return ret;
		}
	}

	if (condition & G_IO_HUP) {
		kill_pending_client(pc);
		return FALSE;	
	}
	
	return TRUE;
}



static gboolean handle_new_client(GIOChannel *c_server, GIOCondition condition, void *_listener)
{
	struct irc_listener *listener = _listener;
	struct pending_client *pc;
	GIOChannel *c;
	int sock = accept(g_io_channel_unix_get_fd(c_server), NULL, 0);

	if (sock < 0) {
		listener_log(LOG_WARNING, listener, "Error accepting new connection: %s", strerror(errno));
		return TRUE;
	}

	c = g_io_channel_unix_new(sock);

	if (listener->config->ssl) {
#ifdef HAVE_GNUTLS
		c = ssl_wrap_iochannel(c, SSL_TYPE_SERVER, 
							 NULL, listener->config->ssl_credentials);
		g_assert(c != NULL);
#else
		listener_log(LOG_WARNING, listener, "SSL support not available, not listening for SSL connection");
#endif
	}

	g_io_channel_set_close_on_unref(c, TRUE);
	g_io_channel_set_encoding(c, NULL, NULL);
	g_io_channel_set_flags(c, G_IO_FLAG_NONBLOCK, NULL);
	pc = g_new0(struct pending_client, 1);
	pc->connection = c;
	pc->watch_id = g_io_add_watch(c, G_IO_IN | G_IO_HUP, handle_client_receive, pc);
	pc->listener = listener;

	g_io_channel_unref(c);

	listener->pending = g_list_append(listener->pending, pc);

	return TRUE;
}



/**
 * Start a listener.
 *
 * @param l Listener to start.
 */
gboolean start_listener(struct irc_listener *l, const char *address, const char *port)
{
	int sock = -1;
	const int on = 1;
	struct addrinfo *res, *all_res;
	int error;
	struct addrinfo hints;
	struct listener_iochannel *lio;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

#ifdef AI_ADDRCONFIG
	hints.ai_flags |= AI_ADDRCONFIG;
#endif

	g_assert(!l->active);

	if (port == NULL)
		port = DEFAULT_IRC_PORT;

	error = getaddrinfo(address, port, &hints, &all_res);
	if (error) {
		listener_log(LOG_ERROR, l, "Can't get address for %s:%s", address?address:"", port);
		return FALSE;
	}

	for (res = all_res; res; res = res->ai_next) {
		GIOChannel *ioc;

		lio = g_new0(struct listener_iochannel, 1);

		if (getnameinfo(res->ai_addr, res->ai_addrlen, 
						lio->address, NI_MAXHOST, lio->port, NI_MAXSERV, 
						NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
			strcpy(lio->address, "");
			strcpy(lio->port, "");
		}

		sock = socket(PF_INET, SOCK_STREAM, 0);
		if (sock < 0) {
			listener_log(LOG_ERROR, l, "error creating socket: %s", 
						 strerror(errno));
			close(sock);
			g_free(lio);
			continue;
		}

		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	
		if (bind(sock, res->ai_addr, res->ai_addrlen) < 0) {
			/* Don't warn when binding to the same address using IPv4 
			 * /and/ ipv6. */
			if (!l->active || errno != EADDRINUSE)  {
				listener_log(LOG_ERROR, l, "bind to %s:%s failed: %s", 
						   address?address:"", port, 
						   strerror(errno));
			}
			close(sock);
			g_free(lio);
			continue;
		}

		if (listen(sock, 5) < 0) {
			listener_log(LOG_ERROR, l, "error listening on socket: %s", 
						strerror(errno));
			close(sock);
			g_free(lio);
			continue;
		}

		ioc = g_io_channel_unix_new(sock);

		if (ioc == NULL) {
			listener_log(LOG_ERROR, l,
						"Unable to create GIOChannel for server socket");
			close(sock);
			g_free(lio);
			continue;
		}

		g_io_channel_set_close_on_unref(ioc, TRUE);

		g_io_channel_set_encoding(ioc, NULL, NULL);
		lio->watch_id = g_io_add_watch(ioc, G_IO_IN, handle_new_client, l);
		g_io_channel_unref(ioc);
		l->incoming = g_list_append(l->incoming, lio);

		listener_log(LOG_INFO, l, "Listening on %s:%s", 
						lio->address, lio->port);
		l->active = TRUE;
	}

	freeaddrinfo(all_res);
	
	return l->active;
}


/* TODO:
 *  - support for ipv4 and ipv6 atyp's
 *  - support for gssapi method
 *  - support for ident method
 */

static gboolean socks_reply(GIOChannel *ioc, guint8 err, guint8 atyp, guint8 data_len, gchar *data, guint16 port)
{
	gchar *header = g_new0(gchar, 7 + data_len);
	GIOStatus status;
	gsize read;

	header[0] = SOCKS_VERSION;
	header[1] = err;
	header[2] = 0x0; /* Reserved */
	header[3] = atyp;
	memcpy(header+4, data, data_len);
	*((guint16 *)(header+4+data_len)) = htons(port);

	status = g_io_channel_write_chars(ioc, header, 6 + data_len, &read, NULL);

	g_free(header);

	g_io_channel_flush(ioc, NULL);

	return (err == REP_OK);
}

static gboolean socks_error(GIOChannel *ioc, guint8 err)
{
	guint8 data = 0x0;
	return socks_reply(ioc, err, ATYP_FQDN, 1, (gchar *)&data, 0);
}

static gboolean anon_acceptable(struct pending_client *cl)
{
	return FALSE; /* Don't allow anonymous connects */
}

static gboolean pass_acceptable(struct pending_client *cl)
{
	return TRUE; /* FIXME: Check whether there is a password specified */
}

static gboolean pass_handle_data(struct pending_client *cl)
{
	GList *gl;
	gchar header[2];
	gsize read;
	GIOStatus status;
	gchar uname[0x100], pass[0x100];

	status = g_io_channel_read_chars(cl->connection, header, 2, &read, NULL);
	if (status != G_IO_STATUS_NORMAL) {
		return FALSE;
	}

	if (header[0] != SOCKS_VERSION && header[0] != 0x1) {
		listener_log(LOG_WARNING, cl->listener, "Client suddenly changed socks uname/pwd version to %x", header[0]);
	 	return socks_error(cl->connection, REP_GENERAL_FAILURE);
	}

	status = g_io_channel_read_chars(cl->connection, uname, header[1], &read, NULL);
	if (status != G_IO_STATUS_NORMAL) {
		return FALSE;
	}

	uname[(guint8)header[1]] = '\0';

	status = g_io_channel_read_chars(cl->connection, header, 1, &read, NULL);
	if (status != G_IO_STATUS_NORMAL) {
		return FALSE;
	}

	status = g_io_channel_read_chars(cl->connection, pass, header[0], &read, NULL);
	if (status != G_IO_STATUS_NORMAL) {
		return FALSE;
	}

	pass[(guint8)header[0]] = '\0';

	header[0] = 0x1;
	header[1] = 0x1; /* set to non-zero if invalid */

	for (gl = cl->listener->config->allow_rules; gl; gl = gl->next)
	{
		struct allow_rule *r = gl->data;

		if (r->password == NULL || r->username == NULL) 
			continue;

		if (strcmp(r->username, uname)) 
			continue;

		if (strcmp(r->password, pass))
			continue;

		header[1] = 0x0;
		break;
	}

	if (cl->listener->config->password == NULL) {
		listener_log(LOG_WARNING, cl->listener, "No password set, allowing client _without_ authentication!");
	}

	if (cl->listener->config->password == NULL ||
		strcmp(cl->listener->config->password, pass) == 0) {
		header[1] = 0x0;
	}

	status = g_io_channel_write_chars(cl->connection, header, 2, &read, NULL);
	if (status != G_IO_STATUS_NORMAL) {
		return FALSE;
	} 

	g_io_channel_flush(cl->connection, NULL);

	if (header[1] == 0x0) {
		cl->socks.state = SOCKS_STATE_NORMAL;		
		return TRUE;
	} else {
		listener_log(LOG_WARNING, cl->listener, "Password mismatch for user %s", uname);
		return FALSE;
	}
}

/**
 * Socks methods.
 */
static struct socks_method {
	gint id;
	const char *name;
	gboolean (*acceptable) (struct pending_client *cl);
	gboolean (*handle_data) (struct pending_client *cl);
} socks_methods[] = {
	{ SOCKS_METHOD_NOAUTH, "none", anon_acceptable, NULL },
	{ SOCKS_METHOD_GSSAPI, "gssapi", NULL, NULL },
	{ SOCKS_METHOD_USERNAME_PW, "username/password", pass_acceptable, pass_handle_data },
	{ -1, NULL, NULL }
};




static gboolean handle_client_socks_data(GIOChannel *ioc, struct pending_client *cl)
{
	GIOStatus status;

	if (cl->socks.state == SOCKS_STATE_NEW) {
		int i;
		gchar header[2];
		gchar methods[0x100];
		gsize read;
		status = g_io_channel_read_chars(ioc, header, 1, &read, NULL);
		if (status != G_IO_STATUS_NORMAL) {
			return FALSE;
		}

		/* None by default */
		cl->socks.method = NULL;

		status = g_io_channel_read_chars(ioc, methods, header[0], &read, NULL);
		if (status != G_IO_STATUS_NORMAL) {
			return FALSE;
		}

		for (i = 0; i < header[0]; i++) {
			int j;
			for (j = 0; socks_methods[j].id != -1; j++)
			{
				if (socks_methods[j].id == methods[i] && 
					socks_methods[j].acceptable != NULL &&
					socks_methods[j].acceptable(cl)) {
					cl->socks.method = &socks_methods[j];
					break;
				}
			}
		}

		header[0] = SOCKS_VERSION;
		header[1] = cl->socks.method?cl->socks.method->id:SOCKS_METHOD_NOACCEPTABLE;

		status = g_io_channel_write_chars(ioc, header, 2, &read, NULL);
		if (status != G_IO_STATUS_NORMAL) {
			return FALSE;
		} 

		g_io_channel_flush(ioc, NULL);

		if (!cl->socks.method) {
			listener_log(LOG_WARNING, cl->listener, "Refused client because no valid method was available");
			return FALSE;
		}

		listener_log(LOG_INFO, cl->listener, "Accepted socks client authenticating using %s", cl->socks.method->name);

		if (!cl->socks.method->handle_data) {
			cl->socks.state = SOCKS_STATE_NORMAL;
		} else {
			cl->socks.state = SOCKS_STATE_AUTH;
		}
	} else if (cl->socks.state == SOCKS_STATE_AUTH) {
		return cl->socks.method->handle_data(cl);
	} else if (cl->socks.state == SOCKS_STATE_NORMAL) {
		gchar header[4];
		gsize read;

		status = g_io_channel_read_chars(ioc, header, 4, &read, NULL);
		if (status != G_IO_STATUS_NORMAL) {
			return FALSE;
		}

		if (header[0] != SOCKS_VERSION) {
			listener_log(LOG_WARNING, cl->listener, "Client suddenly changed socks version to %x", header[0]);
		 	return socks_error(ioc, REP_GENERAL_FAILURE);
		}

		if (header[1] != CMD_CONNECT) {
			listener_log(LOG_WARNING, cl->listener, "Client used unknown command %x", header[1]);
			return socks_error(ioc, REP_CMD_NOT_SUPPORTED);
		}

		/* header[2] is reserved */
	
		switch (header[3]) {
			case ATYP_IPV4: 
				return socks_error(ioc, REP_ATYP_NOT_SUPPORTED);

			case ATYP_IPV6:
				return socks_error(ioc, REP_ATYP_NOT_SUPPORTED);

			case ATYP_FQDN:
				{
					char hostname[0x100];
					guint16 port;
					char *desc;
					struct irc_network *result;
					
					status = g_io_channel_read_chars(ioc, header, 1, &read, NULL);
					status = g_io_channel_read_chars(ioc, hostname, header[0], &read, NULL);
					hostname[(guint8)header[0]] = '\0';

					status = g_io_channel_read_chars(ioc, header, 2, &read, NULL);
					port = ntohs(*(guint16 *)header);

					listener_log(LOG_INFO, cl->listener, "Request to connect to %s:%d", hostname, port);

					result = find_network_by_hostname(cl->listener->global, hostname, port, TRUE);

					if (result == NULL) {
						listener_log(LOG_WARNING, cl->listener, "Unable to return network matching %s:%d", hostname, port);
						return socks_error(ioc, REP_NET_UNREACHABLE);
					} 

					if (result->connection.state == NETWORK_CONNECTION_STATE_NOT_CONNECTED && 
						!connect_network(result)) {
						network_log(LOG_ERROR, result, "Unable to connect");
						return socks_error(ioc, REP_NET_UNREACHABLE);
					}

					if (result->config->type == NETWORK_TCP) {
						struct sockaddr *name; 
						int atyp, len, port;
						gchar *data;

						name = (struct sockaddr *)result->connection.data.tcp.local_name;

						if (name->sa_family == AF_INET) {
							struct sockaddr_in *name4 = (struct sockaddr_in *)name;
							atyp = ATYP_IPV4;
							data = (gchar *)&name4->sin_addr;
							len = 4;
							port = name4->sin_port;
						} else if (name->sa_family == AF_INET6) {
							struct sockaddr_in6 *name6 = (struct sockaddr_in6 *)name;
							atyp = ATYP_IPV6;
							data = (gchar *)&name6->sin6_addr;
							len = 16;
							port = name6->sin6_port;
						} else {
							network_log(LOG_ERROR, result, "Unable to obtain local address for connection to server");
							return socks_error(ioc, REP_NET_UNREACHABLE);
						}
							
						socks_reply(ioc, REP_OK, atyp, len, data, port); 
						
					} else {
						gchar *data = g_strdup("xlocalhost");
						data[0] = strlen(data+1);
						
						socks_reply(ioc, REP_OK, ATYP_FQDN, data[0]+1, data, 1025);
					}

					desc = g_io_channel_ip_get_description(ioc);
					client_init(result, ioc, desc);
					g_free(desc);

					return FALSE;
				}
			default:
				return socks_error(ioc, REP_ATYP_NOT_SUPPORTED);
		}
	}
	
	return TRUE;
}
