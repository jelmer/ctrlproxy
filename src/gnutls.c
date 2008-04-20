/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * soup-gnutls.c
 *
 * Authors:
 *      Ian Peters <itp@ximian.com>
 *
 * Modified for ctrlproxy by Jelmer Vernooij <jelmer@samba.org>
 *
 * Copyright (C) 2003, Ximian, Inc.
 */

#include "internals.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <gcrypt.h>
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

#include "ssl.h"

gboolean ssl_supported = TRUE;

#define DH_BITS 1024

typedef struct {
	gnutls_certificate_credentials cred;
	gboolean have_ca_file;
} GNUTLSCred;

typedef struct {
	GIOChannel channel;
	int fd;
	GIOChannel *real_sock;
	gnutls_session session;
	GNUTLSCred *cred;
	char *hostname;
	gboolean established;
	SSLType type;
} GNUTLSChannel;

static gboolean
verify_certificate (gnutls_session session, const char *hostname, GError **err)
{
	int status;

	status = gnutls_certificate_verify_peers (session);

	if (status == GNUTLS_E_NO_CERTIFICATE_FOUND) {
		g_set_error (err, SSL_ERROR,
			     SSL_ERROR_CERTIFICATE,
			     "No SSL certificate was sent.");
		return FALSE;
	}

	if (status & GNUTLS_CERT_INVALID ||
#ifdef GNUTLS_CERT_NOT_TRUSTED
	    status & GNUTLS_CERT_NOT_TRUSTED ||
#endif
	    status & GNUTLS_CERT_REVOKED)
	{
		g_set_error (err, SSL_ERROR,
			     SSL_ERROR_CERTIFICATE,
			     "The SSL certificate is not trusted.");
		return FALSE;
	}

	if (gnutls_certificate_expiration_time_peers (session) < time (0)) {
		g_set_error (err, SSL_ERROR,
			     SSL_ERROR_CERTIFICATE,
			     "The SSL certificate has expired.");
		return FALSE;
	}

	if (gnutls_certificate_activation_time_peers (session) > time (0)) {
		g_set_error (err, SSL_ERROR,
			     SSL_ERROR_CERTIFICATE,
			     "The SSL certificate is not yet activated.");
		return FALSE;
	}

	if (gnutls_certificate_type_get (session) == GNUTLS_CRT_X509) {
		const gnutls_datum* cert_list;
		guint cert_list_size;
		gnutls_x509_crt cert;

		if (gnutls_x509_crt_init (&cert) < 0) {
			g_set_error (err, SSL_ERROR,
				     SSL_ERROR_CERTIFICATE,
				     "Error initializing SSL certificate.");
			return FALSE;
		}
      
		cert_list = gnutls_certificate_get_peers (
			session, &cert_list_size);

		if (cert_list == NULL) {
			g_set_error (err, SSL_ERROR,
				     SSL_ERROR_CERTIFICATE,
				     "No SSL certificate was found.");
			return FALSE;
		}

		if (gnutls_x509_crt_import (cert, &cert_list[0],
					    GNUTLS_X509_FMT_DER) < 0) {
			g_set_error (err, SSL_ERROR,
				     SSL_ERROR_CERTIFICATE,
				     "The SSL certificate could not be parsed.");
			return FALSE;
		}

		if (!gnutls_x509_crt_check_hostname (cert, hostname)) {
			g_set_error (err, SSL_ERROR,
				     SSL_ERROR_CERTIFICATE,
				     "The SSL certificate does not match the hostname.");
			return FALSE;
		}
	}

	return TRUE;
}

#define GNUTLS_CHANNEL_NONBLOCKING(chan) (fcntl ((chan)->fd, F_GETFL, 0) & O_NONBLOCK)

static GIOStatus
do_handshake (GNUTLSChannel *chan, GError **err)
{
	int result;

again:
	result = gnutls_handshake (chan->session);

	if (result == GNUTLS_E_AGAIN ||
	    result == GNUTLS_E_INTERRUPTED) {
		if (GNUTLS_CHANNEL_NONBLOCKING (chan)) {
			g_set_error (err, SSL_ERROR,
				    (gnutls_record_get_direction (chan->session) ?
				     SSL_ERROR_HANDSHAKE_NEEDS_WRITE :
				     SSL_ERROR_HANDSHAKE_NEEDS_READ),
				    "Handshaking...");
			return G_IO_STATUS_AGAIN;
		} else {
			goto again;
		}
	}

	if (result < 0) {
		g_set_error (err, G_IO_CHANNEL_ERROR,
			     G_IO_CHANNEL_ERROR_FAILED,
			     "Unable to handshake %s", gnutls_strerror(result));

		return G_IO_STATUS_ERROR;
	}

	if (chan->type == SSL_TYPE_CLIENT && chan->cred->have_ca_file &&
	    !verify_certificate (chan->session, chan->hostname, err))
		return G_IO_STATUS_ERROR;

	return G_IO_STATUS_NORMAL;
}

static GIOStatus
_gnutls_read (GIOChannel   *channel,
		  gchar        *buf,
		  gsize         count,
		  gsize        *bytes_read,
		  GError      **err)
{
	GNUTLSChannel *chan = (GNUTLSChannel *) channel;
	gint result;

	*bytes_read = 0;

again:
	if (!chan->established) {
		result = do_handshake (chan, err);

		if (result == G_IO_STATUS_AGAIN ||
		    result == G_IO_STATUS_ERROR)
			return result;

		g_assert(result >= 0);

		chan->established = TRUE;
	}

	result = gnutls_record_recv (chan->session, buf, count);

	if (result == GNUTLS_E_REHANDSHAKE) {
		chan->established = FALSE;
		goto again;
	}

	if ((result == GNUTLS_E_INTERRUPTED) ||
	    (result == GNUTLS_E_AGAIN)) {
		if (GNUTLS_CHANNEL_NONBLOCKING (chan)) {
			return G_IO_STATUS_AGAIN;
		} else {
			goto again;
		}
	}

	if (result < 0) {
		g_set_error (err, G_IO_CHANNEL_ERROR,
			     G_IO_CHANNEL_ERROR_FAILED,
			     "Received corrupted data");
		return G_IO_STATUS_ERROR;
	} else {
		*bytes_read = result;

		return (result > 0) ? G_IO_STATUS_NORMAL : G_IO_STATUS_EOF;
	}
}

static GIOStatus
_gnutls_write (GIOChannel   *channel,
		   const gchar  *buf,
		   gsize         count,
		   gsize        *bytes_written,
		   GError      **err)
{
	GNUTLSChannel *chan = (GNUTLSChannel *) channel;
	gint result;

	*bytes_written = 0;

again:
	if (!chan->established) {
		result = do_handshake (chan, err);

		if (result == G_IO_STATUS_AGAIN ||
		    result == G_IO_STATUS_ERROR)
			return result;


		g_assert(result >= 0);

		chan->established = TRUE;
	}

	result = gnutls_record_send (chan->session, buf, count);

	/* This can't actually happen in response to a write, but... */

	if (result == GNUTLS_E_REHANDSHAKE) {
		chan->established = FALSE;
		goto again;
	}

	if ((result == GNUTLS_E_INTERRUPTED) ||
	    (result == GNUTLS_E_AGAIN)) {
		if (GNUTLS_CHANNEL_NONBLOCKING (chan)) {
			return G_IO_STATUS_AGAIN;
		} else {
			goto again;
		}
	}

	if (result < 0) {
		g_set_error (err, G_IO_CHANNEL_ERROR,
			     G_IO_CHANNEL_ERROR_FAILED,
			     "Received corrupted data");
		return G_IO_STATUS_ERROR;
	} else {
		*bytes_written = result;

		return G_IO_STATUS_NORMAL;
	}
}

static GIOStatus
gnutls_seek (GIOChannel  *channel,
		  gint64       offset,
		  GSeekType    type,
		  GError     **err)
{
	GNUTLSChannel *chan = (GNUTLSChannel *) channel;

	return chan->real_sock->funcs->io_seek (channel, offset, type, err);
}

static GIOStatus
gnutls_close (GIOChannel  *channel,
		   GError     **err)
{
	GNUTLSChannel *chan = (GNUTLSChannel *) channel;

	if (chan->established) {
		int ret;

		do {
			ret = gnutls_bye (chan->session, GNUTLS_SHUT_WR);
		} while (ret == GNUTLS_E_INTERRUPTED);
	}

	return chan->real_sock->funcs->io_close (channel, err);
}

static GSource *
gnutls_create_watch (GIOChannel   *channel,
			  GIOCondition  condition)
{
	GNUTLSChannel *chan = (GNUTLSChannel *) channel;

	return chan->real_sock->funcs->io_create_watch (channel,
							condition);
}

static void
_gnutls_free (GIOChannel *channel)
{
	GNUTLSChannel *chan = (GNUTLSChannel *) channel;
	g_io_channel_unref (chan->real_sock);
	gnutls_deinit (chan->session);
	g_free (chan->hostname);
	g_free (chan);
}

static GIOStatus
gnutls_set_flags (GIOChannel  *channel,
		       GIOFlags     flags,
		       GError     **err)
{
	GNUTLSChannel *chan = (GNUTLSChannel *) channel;

	return chan->real_sock->funcs->io_set_flags (channel, flags, err);
}

static GIOFlags
gnutls_get_flags (GIOChannel *channel)
{
	GNUTLSChannel *chan = (GNUTLSChannel *) channel;

	return chan->real_sock->funcs->io_get_flags (channel);
}

static const GIOFuncs gnutls_channel_funcs = {
	_gnutls_read,
	_gnutls_write,
	gnutls_seek,
	gnutls_close,
	gnutls_create_watch,
	_gnutls_free,
	gnutls_set_flags,
	gnutls_get_flags
};

static gnutls_dh_params dh_params = NULL;

static gboolean
init_dh_params (void)
{
	if (gnutls_dh_params_init (&dh_params) != 0)
		goto THROW_CREATE_ERROR;

	if (gnutls_dh_params_generate2 (dh_params, DH_BITS) != 0)
		goto THROW_CREATE_ERROR;

	return TRUE;

THROW_CREATE_ERROR:
	if (dh_params) {
		gnutls_dh_params_deinit (dh_params);
		dh_params = NULL;
	}

	return FALSE;
}

/**
 * ssl_wrap_iochannel:
 * @sock: a #GIOChannel wrapping a TCP socket.
 * @type: whether this is a client or server socket
 * @remote_host: the hostname of the remote machine
 * @credentials: a client or server credentials structure
 *
 * This attempts to wrap a new #GIOChannel around @sock that
 * will SSL-encrypt/decrypt all traffic through it.
 *
 * Return value: an SSL-encrypting #GIOChannel, or %NULL on
 * failure.
 **/
GIOChannel *
ssl_wrap_iochannel (GIOChannel *sock, SSLType type,
			 const char *remote_host, gpointer credentials)
{
	GNUTLSChannel *chan = NULL;
	GIOChannel *gchan = NULL;
	gnutls_session session = NULL;
	GNUTLSCred *cred = (GNUTLSCred *) credentials;
	int sockfd;
	int ret;

	g_return_val_if_fail (sock != NULL, NULL);
	g_return_val_if_fail (credentials != NULL, NULL);

	g_io_channel_set_encoding(sock, NULL, NULL);
	g_io_channel_set_buffered(sock, FALSE);

	sockfd = g_io_channel_unix_get_fd (sock);
	if (!sockfd) {
		g_warning ("Failed to get channel fd.");
		goto THROW_CREATE_ERROR;
	}

	ret = gnutls_init (&session,
			   (type == SSL_TYPE_CLIENT) ? GNUTLS_CLIENT : GNUTLS_SERVER);
	if (ret)
		goto THROW_CREATE_ERROR;

	if (gnutls_set_default_priority (session) != 0)
		goto THROW_CREATE_ERROR;

	if (gnutls_credentials_set (session, GNUTLS_CRD_CERTIFICATE,
				    cred->cred) != 0)
		goto THROW_CREATE_ERROR;

	if (type == SSL_TYPE_SERVER)
		gnutls_dh_set_prime_bits (session, DH_BITS);

	gnutls_transport_set_ptr (session, GINT_TO_POINTER (sockfd));

	chan = g_new0 (GNUTLSChannel, 1);
	chan->fd = sockfd;
	chan->real_sock = sock;
	chan->session = session;
	chan->cred = cred;
	chan->hostname = g_strdup (remote_host);
	chan->type = type;
	g_io_channel_ref (sock);

	gchan = (GIOChannel *) chan;
	gchan->funcs = (GIOFuncs *) &gnutls_channel_funcs;
	g_io_channel_init (gchan);
	gchan->is_readable = gchan->is_writeable = TRUE;

	return gchan;

 THROW_CREATE_ERROR:
	if (session)
		gnutls_deinit (session);
	return NULL;
}

static gboolean gnutls_inited = FALSE;

static void
_gnutls_init (void)
{
	/* to disallow usage of the blocking /dev/random */
	gcry_control (GCRYCTL_ENABLE_QUICK_RANDOM, 0);

	gnutls_global_init ();
	gnutls_inited = TRUE;
}

/**
 * ssl_get_client_credentials:
 * @ca_file: path to a file containing X509-encoded Certificate
 * Authority certificates.
 *
 * Creates an opaque client credentials object which can later be
 * passed to ssl_wrap_iochannel().
 *
 * If @ca_file is non-%NULL, any certificate received from a server
 * must be signed by one of the CAs in the file, or an error will
 * be returned.
 *
 * Return value: the client credentials, which must be freed with
 * ssl_free_client_credentials().
 **/
gpointer
ssl_get_client_credentials (const char *ca_file)
{
	GNUTLSCred *cred;
	int status;

	if (!gnutls_inited)
		_gnutls_init ();

	cred = g_new0 (GNUTLSCred, 1);
	gnutls_certificate_allocate_credentials (&cred->cred);

	if (ca_file) {
		cred->have_ca_file = TRUE;
		status = gnutls_certificate_set_x509_trust_file (
			cred->cred, ca_file, GNUTLS_X509_FMT_PEM);
		if (status < 0) {
			g_warning ("Failed to set SSL trust file (%s).",
				   ca_file);
			/* Since we set have_ca_file though, this just
			 * means that no certs will validate, so we're
			 * ok securitywise if we just return these
			 * creds to the caller.
			 */
		}
	}

	return cred;
}

/**
 * ssl_free_client_credentials:
 * @creds: a client credentials structure returned by
 * ssl_get_client_credentials().
 *
 * Frees @client_creds.
 **/
void
ssl_free_client_credentials (gpointer creds)
{
	GNUTLSCred *cred = creds;

	gnutls_certificate_free_credentials (cred->cred);
	g_free (cred);
}

/**
 * ssl_get_server_credentials:
 * @cert_file: path to a file containing an X509-encoded server
 * certificate
 * @key_file: path to a file containing an X509-encoded key for
 * @cert_file.
 *
 * Creates an opaque server credentials object which can later be
 * passed to ssl_wrap_iochannel().
 *
 * Return value: the server credentials, which must be freed with
 * ssl_free_server_credentials().
 **/
gpointer
ssl_get_server_credentials (const char *cert_file, const char *key_file)
{
	GNUTLSCred *cred;

	if (!gnutls_inited)
		_gnutls_init ();
	if (!dh_params) {
		if (!init_dh_params ())
			return NULL;
	}

	cred = g_new0 (GNUTLSCred, 1);
	gnutls_certificate_allocate_credentials (&cred->cred);

	if (gnutls_certificate_set_x509_key_file (cred->cred,
						  cert_file, key_file,
						  GNUTLS_X509_FMT_PEM) != 0) {
		g_warning ("Failed to set SSL certificate and key files "
			   "(%s, %s).", cert_file, key_file);
		ssl_free_server_credentials (cred);
		return NULL;
	}

	gnutls_certificate_set_dh_params (cred->cred, dh_params);
	return cred;
}

/**
 * ssl_free_server_credentials:
 * @creds: a server credentials structure returned by
 * ssl_get_server_credentials().
 *
 * Frees @server_creds.
 **/
void
ssl_free_server_credentials (gpointer creds)
{
	GNUTLSCred *cred = creds;

	gnutls_certificate_free_credentials (cred->cred);
	g_free (cred);
}

/**
 * soup_ssl_error_quark:
 *
 * Return value: The quark used as %SOUP_SSL_ERROR
 **/
GQuark
ssl_error_quark (void)
{
	static GQuark error;
	if (!error)
		error = g_quark_from_static_string ("ssl_error_quark");
	return error;
}
