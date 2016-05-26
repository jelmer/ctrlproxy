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

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#include <netdb.h>
#include "socks.h"

#ifdef HAVE_GSSAPI
static gboolean gssapi_fail(struct pending_client *pc);
#endif

struct listener_iochannel {
	struct irc_listener *listener;
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
#ifdef HAVE_GSSAPI
	guint32 major_status, minor_status;
#endif

	pc->listener->pending = g_list_remove(pc->listener->pending, pc);

	g_source_remove(pc->watch_id);

#ifdef HAVE_GSSAPI
	if (pc->authn_name != GSS_C_NO_NAME) {
		major_status = gss_release_name(&minor_status, &pc->authn_name);

		if (GSS_ERROR(major_status)) {
			log_gssapi(pc->listener, LOG_WARNING,
					   "releasing name", major_status, minor_status);
			gssapi_fail(pc);
		}
	}

	if (pc->gss_ctx != GSS_C_NO_CONTEXT) {
		major_status = gss_delete_sec_context(&minor_status,
						      &pc->gss_ctx,
						      GSS_C_NO_BUFFER);
		if (GSS_ERROR(major_status)) {
			log_gssapi(pc->listener, LOG_WARNING,
					   "deleting context name", major_status, minor_status);
			gssapi_fail(pc);
		}
	}

	if (pc->service_cred != GSS_C_NO_CREDENTIAL) {
		major_status = gss_release_cred(&minor_status, &pc->service_cred);
		if (GSS_ERROR(major_status)) {
			log_gssapi(pc->listener, LOG_WARNING,
					   "releasing credentials", major_status, minor_status);
			return FALSE;
		}
	}

	if (pc->gss_service != GSS_C_NO_NAME) {
		guint32 major_status, minor_status;
		major_status = gss_release_name(&minor_status, &pc->gss_service);

		if (GSS_ERROR(major_status)) {
			log_gssapi(pc->listener, LOG_WARNING,
					   "releasing name", major_status, minor_status);
			return FALSE;
		}
	}
#endif

	g_free(pc->clientname);

	g_free(pc);

	return TRUE;
}

#ifdef HAVE_GSSAPI
void log_gssapi(struct irc_listener *l, enum log_level level, const char *message, guint32 major_status, guint32 minor_status)
{
	guint32 err_major_status, err_minor_status;
	guint32	msg_ctx = 0;
	gss_buffer_desc major_status_string = GSS_C_EMPTY_BUFFER,
					minor_status_string = GSS_C_EMPTY_BUFFER;

	err_major_status = gss_display_status( &err_minor_status, major_status,
										   GSS_C_GSS_CODE, GSS_C_NO_OID,
										   &msg_ctx, &major_status_string );

	if( !GSS_ERROR( err_major_status ) )
    	err_major_status = gss_display_status( &err_minor_status, minor_status,
											   GSS_C_MECH_CODE, GSS_C_NULL_OID,
											   &msg_ctx, &minor_status_string );

	listener_log( level, l, "GSSAPI: %s: %s, %s", message,
		      (char *)major_status_string.value,
		      (char *)minor_status_string.value);

	gss_release_buffer( &err_minor_status, &major_status_string );
	gss_release_buffer( &err_minor_status, &minor_status_string );
}
#endif


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
	gchar header[2];
	GError *error = NULL;

	status = g_io_channel_read_chars(ioc, header, 1, &read, &error);

	if (status != G_IO_STATUS_NORMAL && status != G_IO_STATUS_AGAIN) {
		if (error != NULL)
			g_error_free(error);
		return FALSE;
	}

	if (header[0] == SOCKS_VERSION) {
		listener_log(LOG_TRACE, pc->listener, "Detected SOCKS.");
		pc->type = CLIENT_TYPE_SOCKS;
		pc->socks.state = SOCKS_STATE_NEW;
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
			GIOStatus status;
			GError *error = NULL;
			struct irc_line *l;

			while ((status = irc_recv_line(c, pc->listener->iconv, &error, &l)) == G_IO_STATUS_NORMAL) {
				gboolean ret;

				if (l == NULL)
					continue;

				ret = pc->listener->ops->handle_client_line(pc, l);

				free_line(l);

				if (!ret) {
					kill_pending_client(pc);
					return FALSE;
				}
			}

			if (status != G_IO_STATUS_AGAIN) {
				kill_pending_client(pc);
				if (error != NULL)
					g_error_free(error);
				return FALSE;
			}
		} else if (pc->type == CLIENT_TYPE_SOCKS) {
			gboolean ret = handle_client_socks_data(c, pc);
			if (!ret)
				kill_pending_client(pc);
			return ret;
		} else if (pc->type == CLIENT_TYPE_QUASSEL) {
			/* TODO: Quassel */
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


/* TODO:
 *  - support for ipv4 and ipv6 atyp's
 *  - support for gssapi method
 *  - support for ident method
 */

gboolean listener_socks_reply(struct pending_client *pc, guint8 err, guint8 atyp, guint8 data_len, gchar *data, guint16 port)
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

	status = g_io_channel_write_chars(pc->connection, header,
									  6 + data_len, &read, NULL);
	g_free(header);

	if (status != G_IO_STATUS_NORMAL) {
		return FALSE;
	}

	g_io_channel_flush(pc->connection, NULL);

	return (err == REP_OK);
}

gboolean listener_socks_error(struct pending_client *pc, guint8 err)
{
	guint8 data = 0x0;
	return listener_socks_reply(pc, err, ATYP_FQDN, 1, (gchar *)&data, 0);
}

static gboolean anon_acceptable(struct pending_client *cl)
{
	return FALSE; /* Don't allow anonymous connects */
}

static gboolean pass_acceptable(struct pending_client *cl)
{
	return TRUE; /* FIXME: Check whether there is a password specified */
}

/* Called when a socks connection setup is complete.
 * @arg cl The pending client
 * @arg accepted Whether the client was accepted (i.e. authentication was successful)
 * @return Whether the connection setup was successful
 */
static gboolean on_socks_pass_accepted(struct pending_client *cl, gboolean accepted)
{
	gchar header[2];
	GIOStatus status;
	gsize read;

	header[0] = 0x1;
	/* set to non-zero if invalid */
	header[1] = accepted?0x0:0x1;

	status = g_io_channel_write_chars(cl->connection, header, 2, &read, NULL);
	if (status != G_IO_STATUS_NORMAL) {
		return FALSE;
	}

	g_io_channel_flush(cl->connection, NULL);

	if (header[1] == 0x0) {
		cl->socks.state = SOCKS_STATE_NORMAL;
		return TRUE;
	} else {
		return FALSE;
	}
}

static gboolean pass_handle_data(struct pending_client *cl)
{
	gchar header[2];
	gsize read;
	gboolean accepted;
	GIOStatus status;
	gchar uname[0x100], pass[0x100];

	status = g_io_channel_read_chars(cl->connection, header, 2, &read, NULL);
	if (status != G_IO_STATUS_NORMAL) {
		return FALSE;
	}

	if (header[0] != SOCKS_VERSION && header[0] != 0x1) {
		listener_log(LOG_WARNING, cl->listener,
					 "Client suddenly changed socks uname/pwd version to %x",
					 header[0]);
	 	return listener_socks_error(cl, REP_GENERAL_FAILURE);
	}

	status = g_io_channel_read_chars(cl->connection, uname, header[1], &read,
									 NULL);
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

	accepted = cl->listener->ops->socks_auth_simple(cl, uname, pass,
													on_socks_pass_accepted);
	return TRUE;
}

#ifdef HAVE_GSSAPI
static gboolean gssapi_acceptable (struct pending_client *pc)
{
	struct irc_listener *l = pc->listener;
	guint32 major_status, minor_status;
	gss_buffer_desc inbuf;
	char *principal_name;
	int fd, error;
	char address[NI_MAXHOST];
	struct sockaddr_storage sockaddr;
	socklen_t sockaddr_len = sizeof(sockaddr);

	fd = g_io_channel_unix_get_fd(pc->connection);

	error = getsockname(fd, (struct sockaddr *)&sockaddr, &sockaddr_len);
	if (error < 0) {
		listener_log(LOG_WARNING, pc->listener,
					 "Unable to get own socket address");
		return FALSE;
	}

	error = getnameinfo((struct sockaddr *)&sockaddr, sockaddr_len,
						address, NI_MAXHOST,
						NULL, 0, NI_NAMEREQD);
	if (error < 0) {
		listener_log(LOG_WARNING, l, "error looking up canonical name: %s",
						 gai_strerror(error));
		return FALSE;
	}

	principal_name = g_strdup_printf("socks@%s", address);

	inbuf.length = strlen(principal_name);
	inbuf.value = principal_name;

	major_status = gss_import_name(&minor_status, &inbuf,
					   GSS_C_NT_HOSTBASED_SERVICE,
					   &pc->gss_service);

	g_free(principal_name);

	if (GSS_ERROR(major_status)) {
		log_gssapi(l, LOG_ERROR, "importing principal name",
				   major_status, minor_status);
		gssapi_fail(pc);
		return FALSE;
	}

	major_status = gss_acquire_cred(&minor_status, pc->gss_service, 0,
					GSS_C_NULL_OID_SET, GSS_C_ACCEPT,
					&pc->service_cred, NULL, NULL);

	if (GSS_ERROR(major_status)) {
		log_gssapi(l, LOG_ERROR, "acquiring service credentials",
				   major_status, minor_status);
		gssapi_fail(pc);

		return FALSE;
	}

	return TRUE;
}

static gboolean gssapi_fail(struct pending_client *pc)
{
	char header[2];
	GIOStatus status;
	gsize read;

	header[0] = 1; /* SOCKS_GSSAPI_VERSION */
	header[1] = 0xff; /* Message abort */

	status = g_io_channel_write_chars(pc->connection, header, 2, &read, NULL);
	if (status != G_IO_STATUS_NORMAL) {
		return FALSE;
	}

	g_io_channel_flush(pc->connection, NULL);

	return FALSE;
}

static gboolean gssapi_handle_data (struct pending_client *pc)
{
	guint32 major_status, minor_status;
	gss_buffer_desc outbuf;
	gss_buffer_desc inbuf;
	gchar header[4];
	gchar *packet;
	gsize read;
	GIOStatus status;
	guint16 len;

	/*
    + ver  | mtyp | len  |       token           |
    + 0x01 | 0x01 | 0x02 | up to 2^16 - 1 octets |
	*/

	status = g_io_channel_read_chars(pc->connection, header, 4, &read, NULL);
	if (status != G_IO_STATUS_NORMAL) {
		return FALSE;
	}

	if (header[0] != 1) {
		listener_log(LOG_WARNING, pc->listener, "Invalid version %d for SOCKS/GSSAPI protocol", header[0]);
		return FALSE;
	}

	if (header[1] != 1) {
		listener_log(LOG_WARNING, pc->listener, "Invalid SOCKS/GSSAPI command %d", header[1]);
		return FALSE;
	}

	len = ntohs(*((uint16_t *)(header+2)));

	inbuf.value = g_malloc(len);
	status = g_io_channel_read_chars(pc->connection, inbuf.value, len, &inbuf.length, NULL);
	if (status != G_IO_STATUS_NORMAL) {
		return FALSE;
	}

	major_status = gss_accept_sec_context (
		&minor_status,
		&pc->gss_ctx,
		pc->service_cred,
		&inbuf,
		GSS_C_NO_CHANNEL_BINDINGS,
		&pc->authn_name,
		NULL, /* mech_type */
		&outbuf,
		NULL, /* ret_flags */
		NULL, /* time_rec */
		NULL  /* delegated_cred_handle */
	);

	g_free(inbuf.value);

	if (GSS_ERROR(major_status)) {
		log_gssapi(pc->listener, LOG_WARNING, "processing incoming data",
				   major_status, minor_status);
		gssapi_fail(pc);
		return FALSE;
	}

	packet = g_malloc(4+outbuf.length);
	packet[0] = SOCKS_VERSION; /* SOCKS_GSSAPI_VERSION */
	packet[1] = 1; /* Authorization message */
	*((uint16_t *)(packet+2)) = htons(outbuf.length);

	memcpy(packet+4, outbuf.value, outbuf.length);

	status = g_io_channel_write_chars(pc->connection, packet, 4+outbuf.length, &read, NULL);
	g_free(packet);

	if (status != G_IO_STATUS_NORMAL) {
		gssapi_fail(pc);
		return FALSE;
	}

	major_status = gss_release_buffer(&minor_status, &outbuf);
	if (GSS_ERROR(major_status)) {
		log_gssapi(pc->listener, LOG_WARNING, "processing incoming data",
				   major_status, minor_status);
		gssapi_fail(pc);
		return FALSE;
	}

	g_io_channel_flush(pc->connection, NULL);

	g_assert(major_status == GSS_S_COMPLETE);

	if (!pc->listener->ops->socks_gssapi(pc, pc->authn_name)) {
		gssapi_fail(pc);
		return FALSE;
	}

	if (major_status == GSS_S_CONTINUE_NEEDED) {
		return TRUE;
	}

	return TRUE;
}

#endif

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
#ifdef HAVE_GSSAPI
	{ SOCKS_METHOD_GSSAPI, "gssapi", gssapi_acceptable, gssapi_handle_data },
#endif
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
		 	return listener_socks_error(cl, REP_GENERAL_FAILURE);
		}

		if (header[1] != CMD_CONNECT) {
			listener_log(LOG_WARNING, cl->listener, "Client used unknown command %x", header[1]);
			return listener_socks_error(cl, REP_CMD_NOT_SUPPORTED);
		}

		/* header[2] is reserved */

		switch (header[3]) {
			case ATYP_IPV4:
				if (cl->listener->ops->socks_connect_ipv4 == NULL)
					return listener_socks_error(cl, REP_ATYP_NOT_SUPPORTED);
				return cl->listener->ops->socks_connect_ipv4(cl);

			case ATYP_IPV6:
				if (cl->listener->ops->socks_connect_ipv6 == NULL)
					return listener_socks_error(cl, REP_ATYP_NOT_SUPPORTED);
				return cl->listener->ops->socks_connect_ipv6(cl);

			case ATYP_FQDN:
				{
					char hostname[0x100];
					guint16 port;
					status = g_io_channel_read_chars(ioc, header, 1, &read, NULL);
					status = g_io_channel_read_chars(ioc, hostname, header[0], &read, NULL);
					hostname[(guint8)header[0]] = '\0';

					status = g_io_channel_read_chars(ioc, header, 2, &read, NULL);
					port = ntohs(*(guint16 *)header);

					if (cl->listener->ops->socks_connect_fqdn == NULL)
						return listener_socks_error(cl, REP_ATYP_NOT_SUPPORTED);
					return cl->listener->ops->socks_connect_fqdn(cl, hostname, port);
				}
			default:
				return listener_socks_error(cl, REP_ATYP_NOT_SUPPORTED);
		}
	}

	return TRUE;
}
