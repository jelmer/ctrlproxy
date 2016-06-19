/*
	ctrlproxy: A modular IRC proxy
	(c) 2005-2016 Jelmer Vernooij <jelmer@jelmer.uk>

	SOCKS support

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

#include "socks.h"

#ifdef HAVE_GSSAPI
static gboolean gssapi_fail(struct pending_client *pc);
#endif

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


gboolean handle_client_socks_data(GIOChannel *ioc, struct pending_client *cl)
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

void free_socks_data(struct pending_client *pc)
{
#ifdef HAVE_GSSAPI
	guint32 major_status, minor_status;

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
		}
	}

	if (pc->gss_service != GSS_C_NO_NAME) {
		guint32 major_status, minor_status;
		major_status = gss_release_name(&minor_status, &pc->gss_service);

		if (GSS_ERROR(major_status)) {
			log_gssapi(pc->listener, LOG_WARNING,
					   "releasing name", major_status, minor_status);
		}
	}
#endif
}
