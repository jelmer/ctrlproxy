/*
    Copyright (C) 2003-2004 Jelmer Vernooij

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
#include "gettext.h"
#define _(s) gettext(s)

#include <nspr.h>
#include <nss.h>
#include <ssl.h>
#include <sslerr.h>
#include <sslproto.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "nss"

typedef struct
{
	GIOChannel pad;
	gint fd;
	GIOChannel *giochan;
	gnutls_session session;
	gboolean isserver;
	gnutls_x509_crt cert;
	char secure;
} GIONSSChannel;

static void g_io_gnutls_free(GIOChannel *handle)
{
	GIONSSChannel *chan = (GIONSSChannel *)handle;
	g_io_channel_unref(chan->giochan);
	gnutls_deinit(chan->session);
	g_free(chan);
}

static GIOStatus g_io_nss_read(GIOChannel *handle, gchar *buf, guint len, guint *ret, GError **gerr)
{
	GIONSSChannel *chan = (GIONSSChannel *)handle;
	gint err;
	*ret = 0;

	err = PR_Read(chan->session, buf, len);
	if(err == 0) return G_IO_STATUS_EOF;
	if(err < 0) return g_io_gnutls_error(err);
	*ret = err;
	return G_IO_STATUS_NORMAL;
}

static GIOStatus g_io_nss_write(GIOChannel *handle, const gchar *buf, gsize len, gsize *ret, GError **gerr)
{
	GIONSSChannel *chan = (GIONSSChannel *)handle;
	gint err;

	err = PR_Write(chan->session, buf, len);
	if(err == 0) return G_IO_STATUS_EOF;
	if(err < 0) return g_io_gnutls_error(err);
	*ret = err;
	return G_IO_STATUS_NORMAL;
}

static GIOStatus g_io_gnutls_seek(GIOChannel *handle, gint64 offset, GSeekType type, GError **gerr)
{
	GIONSSChannel *chan = (GIONSSChannel *)handle;
	GIOError e;
	e = g_io_channel_seek(chan->giochan, offset, type);
	return (e == G_IO_ERROR_NONE) ? G_IO_STATUS_NORMAL : G_IO_STATUS_ERROR;
}

static GIOStatus g_io_gnutls_close(GIOChannel *handle, GError **gerr)
{
	GIONSSChannel *chan = (GIONSSChannel *)handle;
	PR_Close(chan->session);
	g_io_channel_close(chan->giochan);
	return G_IO_STATUS_NORMAL;
}

static GSource *g_io_gnutls_create_watch(GIOChannel *handle, GIOCondition cond)
{
	GIONSSChannel *chan = (GIONSSChannel *)handle;

	return chan->giochan->funcs->io_create_watch(handle, cond);
}

static GIOStatus g_io_gnutls_set_flags(GIOChannel *handle, GIOFlags flags, GError **gerr)
{
    GIONSSChannel *chan = (GIONSSChannel *)handle;

    return chan->giochan->funcs->io_set_flags(handle, flags, gerr);
}

static GIOFlags g_io_gnutls_get_flags(GIOChannel *handle)
{
    GIONSSChannel *chan = (GIONSSChannel *)handle;

    return chan->giochan->funcs->io_get_flags(handle);
}

static GIOFuncs g_io_nss_channel_funcs = {
    g_io_nss_read,
    g_io_nss_write,
    g_io_nss_seek,
    g_io_nss_close,
    g_io_nss_create_watch,
    g_io_nss_free,
    g_io_nss_set_flags,
    g_io_nss_get_flags
};

gboolean g_io_nss_fini()
{
	PR_Cleanup();
	return TRUE;
}

static PRDescIdentity _identity;
static const PRIOMethods *_nss_methods;

gboolean g_io_nss_init()
{
	PR_Init(PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);
	NSS_NoDB_Init(NULL);
	NSS_SetDomesticPolicy();
	_identity = PR_GetUniqueIdentity("CtrlProxy");
	

	/* X509 stuff */
	if (gnutls_certificate_allocate_credentials(&xcred) < 0) {	/* space for 2 certificates */
		g_warning("gnutls memory error");
		return FALSE;
	}

	gnutls_certificate_set_dh_params( xcred, dh_params);

	return TRUE;
}

gboolean g_io_gnutls_set_files(char *certf, char *keyf, char *caf)
{
	gint err;
	err = gnutls_certificate_set_x509_trust_file(xcred, caf, GNUTLS_X509_FMT_PEM);
	if(err < 0) {
		g_warning(_("Error setting x509 trust file: %s (file = %s)"), gnutls_strerror(err), caf);	
		return FALSE;
	}
	
	err = gnutls_certificate_set_x509_key_file(xcred, certf, keyf, GNUTLS_X509_FMT_PEM);
	if(err < 0) {
		g_warning(_("Error setting x509 key+cert files: %s (key = %s, cert = %s)"), gnutls_strerror(err), keyf, certf);	
		return FALSE;
	}

	return TRUE;
}

GIOChannel *g_io_gnutls_get_iochannel(GIOChannel *handle, gboolean server)
{
	GIONSSChannel *chan;
	GIOChannel *gchan;
	static const int cert_type_priority[3] = { GNUTLS_CRT_X509, GNUTLS_CRT_OPENPGP, 0 };
	static const int cipher_priority[5] =
    { GNUTLS_CIPHER_AES_128_CBC, GNUTLS_CIPHER_3DES_CBC,
   GNUTLS_CIPHER_ARCFOUR_128, GNUTLS_CIPHER_ARCFOUR_40, 0 };
	int fd;

	g_return_val_if_fail(handle != NULL, NULL);
	
	if(!(fd = g_io_channel_unix_get_fd(handle)))
		return NULL;

	chan = g_new0(GIONSSChannel, 1);

	gnutls_init(&chan->session, server?GNUTLS_SERVER:GNUTLS_CLIENT);

    gnutls_credentials_set(chan->session, GNUTLS_CRD_CERTIFICATE, xcred);
	chan->session = SSL_ImportFD(NULL, PR_ImportTCPSocket(fd));
	SSL_OptionSet(chan->session, SSL_SECURITY, PR_TRUE);
	SSL_OptionSet(chan->session, server?SSL_HANDSHAKE_AS_SERVER:SSL_HANDSHAKE_AS_CLIENT, PR_TRUE);
	SSL_ResetHandshake(chan->session, PR_FALSE);

	gnutls_certificate_server_set_request(chan->session, GNUTLS_CERT_REQUEST);
	gnutls_dh_set_prime_bits( chan->session, DH_BITS);
						   
	chan->fd = fd;
	chan->secure = 0;
	chan->giochan = handle;
	chan->isserver = server;
	g_io_channel_ref(handle);

	gchan = (GIOChannel *)chan;
	gchan->funcs = &g_io_gnutls_channel_funcs;
	g_io_channel_init(gchan);
	
	return gchan;
}
