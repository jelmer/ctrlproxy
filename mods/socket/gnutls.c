/*
	(Based on network-openssl.c from irssi)

    Copyright (C) 2002 vjt
    Copyright (C) 2003 Jelmer Vernooij

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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <glib.h>
#include "../config.h"
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

static gnutls_certificate_credentials xcred;

/* gnutls i/o channel object */
typedef struct
{
	GIOChannel pad;
	gint fd;
	GIOChannel *giochan;
	gnutls_session session;
	gboolean isserver;
	gnutls_x509_crt cert;
	char secure;
} GIOTLSChannel;
	
static void irssi_gnutls_free(GIOChannel *handle)
{
	GIOTLSChannel *chan = (GIOTLSChannel *)handle;
	g_io_channel_unref(chan->giochan);
	gnutls_deinit(chan->session);
	g_free(chan);
}

static GIOStatus irssi_ssl_errno(gint e)
{
	switch(e)
	{
		case EINVAL:
			return G_IO_STATUS_ERROR;
		case EINTR:
		case EAGAIN:
			return G_IO_STATUS_AGAIN;
		default:
			return G_IO_STATUS_ERROR;
	}
	/*UNREACH*/
	return G_IO_STATUS_ERROR;
}

static GIOStatus gnutls_get_peer_certificate(gnutls_session session, const char* hostname) {
	int status;
	const gnutls_datum* cert_list;
	int cert_list_size;
	gnutls_x509_crt cert;


	/* This verification function uses the trusted CAs in the credentials
	 *     * structure. So you must have installed one or more CA certificates.
	 *         */
	status = gnutls_certificate_verify_peers(session);

	if (status == GNUTLS_E_NO_CERTIFICATE_FOUND) {
		return G_IO_STATUS_ERROR;
	}

	if (status & GNUTLS_CERT_INVALID) 
		printf("The certificate is not trusted.\n");

	if (status & GNUTLS_CERT_SIGNER_NOT_FOUND)
		printf("The certificate hasn't got a known issuer.\n");

	if (status & GNUTLS_CERT_REVOKED)
		printf("The certificate has been revoked.\n");

	/* Up to here the process is the same for X.509 certificates and
	 * OpenPGP keys. From now on X.509 certificates are assumed. This can
	 * be easily extended to work with openpgp keys as well.
	*/

	switch( gnutls_certificate_type_get(session)) {
	case  GNUTLS_CRT_X509:
		if ( gnutls_x509_crt_init( &cert) < 0) {
			printf("error in initialization\n");
			return G_IO_STATUS_ERROR;
		}

		cert_list = gnutls_certificate_get_peers( session, &cert_list_size);
		if ( cert_list == NULL) {
			printf("No certificate was found!\n");
			return G_IO_STATUS_ERROR;
		}

		/* This is not a real world example, since we only check the first 
		 *     * certificate in the given chain.
		 *         */
		if ( gnutls_x509_crt_import( cert, &cert_list[0], GNUTLS_X509_FMT_DER) < 0) {
			printf("error parsing certificate\n");
			return G_IO_STATUS_ERROR;
		}

		/* Beware here we do not check for errors.
		 *     */
		if ( gnutls_x509_crt_get_expiration_time( cert) < time(0)) {
			printf("The certificate has expired\n");
			return G_IO_STATUS_ERROR;
		}

		if ( gnutls_x509_crt_get_activation_time( cert) > time(0)) {
			printf("The certificate is not yet activated\n");
			return G_IO_STATUS_ERROR;
		}

		if ( !gnutls_x509_crt_check_hostname( cert, hostname)) {
			printf("The certificate's owner does not match hostname '%s'\n", hostname);
			return G_IO_STATUS_ERROR;
		}

		gnutls_x509_crt_deinit( cert);

		return G_IO_STATUS_NORMAL;

	case GNUTLS_CRT_OPENPGP:
		printf("Unsupported ATM\n");
		return G_IO_STATUS_ERROR;
	}

	return G_IO_STATUS_ERROR;
}

static GIOStatus irssi_gnutls_cert_step(GIOTLSChannel *chan)
{
	gint err;
	switch(err = gnutls_handshake(chan->session))
	{
		case 0:
			return gnutls_certificate_verify_peers(chan->session);
		case GNUTLS_E_AGAIN:
		case GNUTLS_E_INTERRUPTED:
			return G_IO_STATUS_AGAIN;
		default:
			return irssi_gnutls_errno(err);
	}
	/*UNREACH*/
	return G_IO_STATUS_ERROR;
}

static GIOStatus irssi_gnutls_read(GIOChannel *handle, gchar *buf, guint len, guint *ret, GError **gerr)
{
	GIOTLSChannel *chan = (GIOTLSChannel *)handle;
	gint err;
	
	if(chan->cert == NULL && !chan->isserver)
	{
		gint cert_err = irssi_gnutls_cert_step(chan);
		if(cert_err != G_IO_STATUS_NORMAL)
			return cert_err;
	}
	
	err = gnutls_record_recv(chan->session, buf, len);
	if(err < 0)
	{
		*ret = 0;
		return irssi_gnutls_errno(err);
	} else {
		*ret = err;
		if(err == 0) return G_IO_STATUS_EOF;
		return G_IO_STATUS_NORMAL;
	}
	/*UNREACH*/
	return G_IO_STATUS_ERROR;
}

static GIOStatus irssi_gnutls_write(GIOChannel *handle, const gchar *buf, gsize len, gsize *ret, GError **gerr)
{
	GIOTLSChannel *chan = (GIOTLSChannel *)handle;
	gint err;

	if(chan->secure) 
	{
		err = gnutls_record_send(chan->session, (const char *)buf, len);
		if(err < 0)
		{
			*ret = 0;
			return irssi_gnutls_errno(errno);
		} else {
			*ret = err;
			return G_IO_STATUS_NORMAL;
		}
	} else {
		return /* FIXME */1; 
	}
	/*UNREACH*/
	return G_IO_STATUS_ERROR;
}

static GIOStatus irssi_gnutls_seek(GIOChannel *handle, gint64 offset, GSeekType type, GError **gerr)
{
	GIOTLSChannel *chan = (GIOTLSChannel *)handle;
	GIOError e;
	e = g_io_channel_seek(chan->giochan, offset, type);
	return (e == G_IO_ERROR_NONE) ? G_IO_STATUS_NORMAL : G_IO_STATUS_ERROR;
}

static GIOStatus irssi_gnutls_close(GIOChannel *handle, GError **gerr)
{
	GIOTLSChannel *chan = (GIOTLSChannel *)handle;
	gnutls_bye(chan->session, GNUTLS_SHUT_RDWR);
	g_io_channel_close(chan->giochan);

	return G_IO_STATUS_NORMAL;
}

static GSource *irssi_gnutls_create_watch(GIOChannel *handle, GIOCondition cond)
{
	GIOTLSChannel *chan = (GIOTLSChannel *)handle;

	return chan->giochan->funcs->io_create_watch(handle, cond);
}

static GIOStatus irssi_gnutls_set_flags(GIOChannel *handle, GIOFlags flags, GError **gerr)
{
    GIOTLSChannel *chan = (GIOTLSChannel *)handle;

    return chan->giochan->funcs->io_set_flags(handle, flags, gerr);
}

static GIOFlags irssi_gnutls_get_flags(GIOChannel *handle)
{
    GIOTLSChannel *chan = (GIOTLSChannel *)handle;

    return chan->giochan->funcs->io_get_flags(handle);
}

static GIOFuncs irssi_gnutls_channel_funcs = {
    irssi_gnutls_read,
    irssi_gnutls_write,
    irssi_gnutls_seek,
    irssi_gnutls_close,
    irssi_gnutls_create_watch,
    irssi_gnutls_free,
    irssi_gnutls_set_flags,
    irssi_gnutls_get_flags
};

static gboolean irssi_gnutls_fini()
{
	gnutls_global_deinit();
	return TRUE;
}

static gboolean irssi_gnutls_init()
{

	/* X509 stuff */
	gnutls_certificate_allocate_credentials(&xcred);
	gnutls_certificate_set_verify_flags(xcred, GNUTLS_VERIFY_ALLOW_X509_V1_CA_CRT);

	return TRUE;
}

gboolean irssi_gnutls_set_files(char *certf)
{
	/* sets the trusted cas file */
	gnutls_certificate_set_x509_trust_file(xcred, certf, GNUTLS_X509_FMT_PEM);

	return TRUE;
}

GIOChannel *irssi_gnutls_get_iochannel(GIOChannel *handle, gboolean server)
{
	GIOTLSChannel *chan;
	GIOChannel *gchan;
	int err, fd;
	gnutls_x509_crt cert;

	g_return_val_if_fail(handle != NULL, NULL);
	
	if(!(fd = g_io_channel_unix_get_fd(handle)))
		return NULL;

	chan = g_new0(GIOTLSChannel, 1);

	gnutls_init(&chan->session, server?GNUTLS_SERVER:GNUTLS_CLIENT);
	gnutls_set_default_priority( chan->session);  
    gnutls_credentials_set(chan->session, GNUTLS_CRD_CERTIFICATE, xcred);

	gnutls_transport_set_ptr( chan->session, (gnutls_transport_ptr)fd);

	if(err <= 0)
	{
		const char *buf;
		buf = gnutls_strerror(err);
		g_warning("TLS Error: %s", buf);
		return NULL;
	}
	else if(!(cert = gnutls_get_peer_certificate(chan->session)))
	{
		if(!server)
		{
			g_warning("SSL %s supplied no certificate", server?"client":"server");
			return NULL;
		}
	}
	else
		gnutls_x509_free(cert);

	chan->fd = fd;
	chan->giochan = handle;
	chan->ssl = ssl;
	chan->cert = cert;
	chan->isserver = server;
	g_io_channel_ref(handle);

	gchan = (GIOChannel *)chan;
	gchan->funcs = &irssi_ssl_channel_funcs;
	g_io_channel_init(gchan);
	
	return gchan;
}
