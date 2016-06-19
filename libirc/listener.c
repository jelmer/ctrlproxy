/*
	ctrlproxy: A modular IRC proxy
	(c) 2005-2006 Jelmer Vernooij <jelmer@jelmer.uk>

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
#include "libirc/listener.h"
#include "ssl.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <netdb.h>
#include "socks.h"

struct listener_iochannel {
	struct irc_listener *listener;
	char address[NI_MAXHOST];
	char port[NI_MAXSERV];
	gint watch_id;
};

static gboolean handle_client_detect(GIOChannel *ioc,
									 struct pending_client *cl);
gboolean handle_client_quassel_data(GIOChannel *ioc,
										 struct pending_client *cl);
gboolean listener_probe_socks(gchar *header, size_t length);
gboolean listener_probe_quassel(gchar *header, size_t length);

/* From listener_socks.c */
gboolean handle_client_socks_data(GIOChannel *ioc, struct pending_client *cl);
void free_socks_data(struct pending_client *pc);

static gboolean kill_pending_client(struct pending_client *pc)
{

	pc->listener->pending = g_list_remove(pc->listener->pending, pc);

	g_source_remove(pc->watch_id);

	free_socks_data(pc);

	g_free(pc->clientname);

	g_free(pc);

	return TRUE;
}

gboolean listener_stop(struct irc_listener *l)
{
	while (l->incoming != NULL) {
		struct listener_iochannel *lio = l->incoming->data;

		g_source_remove(lio->watch_id);

		if (strcmp(lio->address, "") != 0)
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
	gchar header[4];
	GError *error = NULL;

	status = g_io_channel_read_chars(ioc, header, 1, &read, &error);

	if (status != G_IO_STATUS_NORMAL && status != G_IO_STATUS_AGAIN) {
		if (error != NULL)
			g_error_free(error);
		return FALSE;
	}

	if (listener_probe_socks(header, 1)) {
		listener_log(LOG_TRACE, pc->listener, "Detected SOCKS.");
		pc->type = CLIENT_TYPE_SOCKS;
		pc->socks.state = SOCKS_STATE_NEW;
		return TRUE;
	} else if (listener_probe_quassel(header, 1)) {
		listener_log(LOG_TRACE, pc->listener, "Detected Quassel.");
		pc->type = CLIENT_TYPE_QUASSEL;
		pc->quassel.state = QUASSEL_STATE_NEW;
		return TRUE;
	} else {
		struct irc_line *l = NULL;
		gchar *raw = NULL, *cvrt = NULL;
		gchar *complete;
		GIOStatus status;
		gboolean ret;
		gsize in_len;

		pc->type = CLIENT_TYPE_REGULAR;

		g_assert(ioc != NULL);

		status = g_io_channel_read_line(ioc, &raw, &in_len, NULL, &error);
		if (status != G_IO_STATUS_NORMAL) {
			g_free(raw);
			g_error_free(error);
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
				if (error != NULL)
					g_error_free(error);
			} else {
				g_free(complete);
			}
		}

		l = irc_parse_line(cvrt);

		ret = pc->listener->ops->handle_client_line(pc, l);

		free_line(l);

		g_free(cvrt);

		return ret;
	}
}

gboolean handle_client_regular_data(GIOChannel *c, struct pending_client *pc)
{
	GIOStatus status;
	GError *error = NULL;
	struct irc_line *l;

	while ((status = irc_recv_line(c, pc->listener->iconv, &error, &l)) == G_IO_STATUS_NORMAL) {
		gboolean ret;

		if (l == NULL) {
			continue;
		}

		ret = pc->listener->ops->handle_client_line(pc, l);

		free_line(l);

		if (!ret) {
			return FALSE;
		}
	}

	if (status != G_IO_STATUS_AGAIN) {
		if (error != NULL)
			g_error_free(error);
		return FALSE;
	}

	return TRUE;
}

static gboolean handle_client_receive(GIOChannel *c, GIOCondition condition, gpointer data)
{
	struct pending_client *pc = data;

	g_assert(c != NULL);

	if (condition & G_IO_IN) {
		if (pc->type == CLIENT_TYPE_UNKNOWN) {
			if (!handle_client_detect(c, pc)) {
				kill_pending_client(pc);
				return FALSE;
			}
		} else if (pc->type == CLIENT_TYPE_REGULAR) {
			gboolean ret = handle_client_regular_data(c, pc);
			if (!ret)
				kill_pending_client(pc);
			return ret;
		} else if (pc->type == CLIENT_TYPE_SOCKS) {
			gboolean ret = handle_client_socks_data(c, pc);
			if (!ret)
				kill_pending_client(pc);
			return ret;
		} else if (pc->type == CLIENT_TYPE_QUASSEL) {
			gboolean ret = handle_client_quassel_data(c, pc);
			if (!ret)
				kill_pending_client(pc);
			return ret;
		} else {
			g_assert(0);
		}
	}

	if (condition & G_IO_HUP) {
		kill_pending_client(pc);
		return FALSE;
	}

	return TRUE;
}

struct pending_client *listener_new_pending_client(struct irc_listener *listener, GIOChannel *c)
{
	struct pending_client *pc;

	if (listener->ssl) {
#ifdef HAVE_GNUTLS
		c = ssl_wrap_iochannel(c, SSL_TYPE_SERVER,
							 NULL, listener->ssl_credentials);
		g_assert(c != NULL);
#else
		listener_log(LOG_WARNING, listener, "SSL support not available, not listening for SSL connection");
#endif
	}

	g_io_channel_set_close_on_unref(c, TRUE);
	g_io_channel_set_encoding(c, NULL, NULL);
	g_io_channel_set_flags(c, G_IO_FLAG_NONBLOCK, NULL);
	pc = g_new0(struct pending_client, 1);
#ifdef HAVE_GSSAPI
	pc->authn_name = GSS_C_NO_NAME;
	pc->gss_ctx = GSS_C_NO_CONTEXT;
	pc->gss_service = GSS_C_NO_NAME;
	pc->service_cred = GSS_C_NO_CREDENTIAL;
#endif
	pc->connection = c;
	pc->watch_id = g_io_add_watch(c, G_IO_IN | G_IO_HUP, handle_client_receive, pc);
	pc->listener = listener;

	g_io_channel_unref(c);

	listener->pending = g_list_append(listener->pending, pc);

	if (listener->ops->new_client)
		listener->ops->new_client(pc);

	return pc;
}

static gboolean handle_new_client(GIOChannel *c_server, GIOCondition condition, void *_listener)
{
	struct irc_listener *listener = _listener;
	GIOChannel *c;
	int sock = accept(g_io_channel_unix_get_fd(c_server), NULL, 0);

	if (sock < 0) {
		listener_log(LOG_WARNING, listener, "Error accepting new connection: %s", strerror(errno));
		return TRUE;
	}

	c = g_io_channel_unix_new(sock);

	listener_new_pending_client(listener, c);

	return TRUE;
}

void listener_add_iochannel(struct irc_listener *l, GIOChannel *ioc,
							const char *address, const char *port)
{
	struct listener_iochannel *lio;

	lio = g_new0(struct listener_iochannel, 1);
	lio->listener = l;

	g_io_channel_set_close_on_unref(ioc, TRUE);
	g_io_channel_set_encoding(ioc, NULL, NULL);
	lio->watch_id = g_io_add_watch(ioc, G_IO_IN, handle_new_client, l);
	g_io_channel_unref(ioc);
	l->incoming = g_list_append(l->incoming, lio);

	if (address == NULL)
		strcpy(lio->address, "");
	else
		strncpy(lio->address, address, NI_MAXHOST);
	if (port == NULL)
		strcpy(lio->address, "");
	else
		strncpy(lio->port, port, NI_MAXSERV);

	if (address != NULL)
		listener_log(LOG_INFO, l, "Listening on %s:%s",
					 address, port);
	l->active = TRUE;
}

/**
 * Start a listener.
 *
 * @param l Listener to start.
 */
gboolean listener_start_tcp(struct irc_listener *l, const char *address, const char *port)
{
	int sock = -1;
	const int on = 1;
	struct addrinfo *res, *all_res;
	int error;
	struct addrinfo hints;

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
		listener_log(LOG_ERROR, l, "Can't get address for %s:%s: %s",
					 address?address:"", port, gai_strerror(error));
		return FALSE;
	}

	for (res = all_res; res; res = res->ai_next) {
		GIOChannel *ioc;
		char canon_address[NI_MAXHOST], canon_port[NI_MAXSERV];

		error = getnameinfo(res->ai_addr, res->ai_addrlen,
							canon_address, NI_MAXHOST,
							canon_port, NI_MAXSERV, NI_NUMERICHOST|NI_NUMERICSERV);
		if (error < 0) {
			listener_log(LOG_WARNING, l, "error looking up canonical name: %s",
						 gai_strerror(error));
			strcpy(canon_address, "");
			strcpy(canon_port, "");
		}

		sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (sock < 0) {
			listener_log(LOG_ERROR, l, "error creating socket: %s",
						 strerror(errno));
			close(sock);
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
			continue;
		}

		if (listen(sock, 5) < 0) {
			listener_log(LOG_ERROR, l, "error listening on socket: %s",
						strerror(errno));
			close(sock);
			continue;
		}

		ioc = g_io_channel_unix_new(sock);

		if (ioc == NULL) {
			listener_log(LOG_ERROR, l,
						"Unable to create GIOChannel for server socket");
			close(sock);
			continue;
		}

		listener_add_iochannel(l, ioc, canon_address, canon_port);
	}

	freeaddrinfo(all_res);

	return l->active;
}


