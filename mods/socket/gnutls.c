/*
	(Based on network-openssl.c from g_io)

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

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "gnutls"

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
	
static void g_io_gnutls_free(GIOChannel *handle)
{
	GIOTLSChannel *chan = (GIOTLSChannel *)handle;
	g_io_channel_unref(chan->giochan);
	gnutls_deinit(chan->session);
	g_free(chan);
}

static GIOStatus g_io_gnutls_read(GIOChannel *handle, gchar *buf, guint len, guint *ret, GError **gerr)
{
	GIOTLSChannel *chan = (GIOTLSChannel *)handle;
	gint err;
	
	err = gnutls_record_recv(chan->session, buf, len);
	/* FIXME */
	if(err < 0)
	{
		*ret = 0;
		return g_io_gnutls_errno(err);
	} else {
		*ret = err;
		if(err == 0) return G_IO_STATUS_EOF;
		return G_IO_STATUS_NORMAL;
	}
	/*UNREACH*/
	return G_IO_STATUS_ERROR;
}

static GIOStatus g_io_gnutls_write(GIOChannel *handle, const gchar *buf, gsize len, gsize *ret, GError **gerr)
{
	GIOTLSChannel *chan = (GIOTLSChannel *)handle;
	gint err;

	if(chan->secure) 
	{
		err = gnutls_record_send(chan->session, (const char *)buf, len);
		/* FIXME*/
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

static GIOStatus g_io_gnutls_seek(GIOChannel *handle, gint64 offset, GSeekType type, GError **gerr)
{
	GIOTLSChannel *chan = (GIOTLSChannel *)handle;
	GIOError e;
	e = g_io_channel_seek(chan->giochan, offset, type);
	return (e == G_IO_ERROR_NONE) ? G_IO_STATUS_NORMAL : G_IO_STATUS_ERROR;
}

static GIOStatus g_io_gnutls_close(GIOChannel *handle, GError **gerr)
{
	GIOTLSChannel *chan = (GIOTLSChannel *)handle;
	gnutls_bye(chan->session, GNUTLS_SHUT_RDWR);
	g_io_channel_close(chan->giochan);
	return G_IO_STATUS_NORMAL;
}

static GSource *g_io_gnutls_create_watch(GIOChannel *handle, GIOCondition cond)
{
	GIOTLSChannel *chan = (GIOTLSChannel *)handle;

	return chan->giochan->funcs->io_create_watch(handle, cond);
}

static GIOStatus g_io_gnutls_set_flags(GIOChannel *handle, GIOFlags flags, GError **gerr)
{
    GIOTLSChannel *chan = (GIOTLSChannel *)handle;

    return chan->giochan->funcs->io_set_flags(handle, flags, gerr);
}

static GIOFlags g_io_gnutls_get_flags(GIOChannel *handle)
{
    GIOTLSChannel *chan = (GIOTLSChannel *)handle;

    return chan->giochan->funcs->io_get_flags(handle);
}

static GIOFuncs g_io_gnutls_channel_funcs = {
    g_io_gnutls_read,
    g_io_gnutls_write,
    g_io_gnutls_seek,
    g_io_gnutls_close,
    g_io_gnutls_create_watch,
    g_io_gnutls_free,
    g_io_gnutls_set_flags,
    g_io_gnutls_get_flags
};

gboolean g_io_gnutls_fini()
{
	gnutls_certificate_free_credentials(xcred);
	gnutls_global_deinit();
	return TRUE;
}

gboolean g_io_gnutls_init()
{
	if(gnutls_global_init() < 0) {
		g_warning("gnutls global state initialization error");
		return FALSE;
	}

    generate_rsa_params();
    generate_dh_primes();

	/* X509 stuff */
	if (gnutls_certificate_allocate_credentials(&xcred) < 0) {	/* space for 2 certificates */
		g_warning("gnutls memory error");
		return FALSE;
	}
	
	gnutls_certificate_set_verify_flags(xcred, GNUTLS_VERIFY_ALLOW_X509_V1_CA_CRT);

	return TRUE;
}

gboolean g_io_gnutls_set_files(char *certf)
{
	gnutls_certificate_set_x509_trust_file(xcred, certf, GNUTLS_X509_FMT_PEM);
	gnutls_certificate_set_x509_key_file(xcred, certf, GNUTLS_X509_FMT_PEM);

	return TRUE;
}

GIOChannel *g_io_gnutls_get_iochannel(GIOChannel *handle, gboolean server)
{
	GIOTLSChannel *chan;
	GIOChannel *gchan;
	int err, fd;

	g_return_val_if_fail(handle != NULL, NULL);
	
	if(!(fd = g_io_channel_unix_get_fd(handle)))
		return NULL;

	chan = g_new0(GIOTLSChannel, 1);

	gnutls_init(&chan->session, server?GNUTLS_SERVER:GNUTLS_CLIENT);
	gnutls_handshake_set_private_extensions(chan->session, 1);
	gnutls_transport_set_ptr(chan->session, (gnutls_transport_ptr)fd);
    gnutls_credentials_set(chan->session, GNUTLS_CRD_CERTIFICATE, xcred);

                                                                               
	gnutls_certificate_server_set_request(chan->session, GNUTLS_CERT_REQUEST);
						   
	chan->fd = fd;
	chan->giochan = handle;
	chan->isserver = server;
	g_io_channel_ref(handle);

	gchan = (GIOChannel *)chan;
	gchan->funcs = &g_io_gnutls_channel_funcs;
	g_io_channel_init(gchan);
	
	return gchan;
}
