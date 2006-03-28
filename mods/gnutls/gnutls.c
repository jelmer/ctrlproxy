/*
    Copyright (C) 2004 Jelmer Vernooij

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

#include "ctrlproxy.h"
#include <glib.h>
#include "../config.h"
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <gnutls/gnutls.h>
#include <gnutls/extra.h>
#include <gnutls/x509.h>

#define DH_BITS 1024

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "gnutls"

static gnutls_certificate_credentials xcred;

GIOChannel *g_io_gnutls_get_iochannel(GIOChannel *handle, gboolean server);

/* gnutls i/o channel object */
typedef struct
{
	GIOChannel pad;
	GIOChannel *giochan;
	gnutls_session session;
	gboolean isserver;
	gnutls_x509_crt cert;
	char secure;
} GIOTLSChannel;

static GIOStatus g_io_gnutls_error(gint e)
{
	switch(e)
	{
		case GNUTLS_E_INTERRUPTED:
		case GNUTLS_E_AGAIN:
			return G_IO_STATUS_AGAIN;
		default:
			log_global("gnutls", LOG_ERROR, "TLS Error: %s", gnutls_strerror(e));
			if(gnutls_error_is_fatal(e)) return G_IO_STATUS_EOF;
			return G_IO_STATUS_ERROR;
	}

	return G_IO_STATUS_ERROR;
}
	
static void g_io_gnutls_free(GIOChannel *handle)
{
	GIOTLSChannel *chan = (GIOTLSChannel *)handle;
	g_io_channel_unref(chan->giochan);
	gnutls_deinit(chan->session);
	g_free(chan);
}

static ssize_t tls_pull(gnutls_transport_ptr ptr, void *buf, size_t size)
{
	GIOChannel *parent = ptr;
	GIOStatus status;
	gint nr;
	status = g_io_channel_read_chars(parent, buf, size, &nr, NULL);
	
	switch (status) {
	case G_IO_STATUS_ERROR:
	case G_IO_STATUS_EOF:
		return -1;
	default:
		return nr;
	}
}

static ssize_t tls_push(gnutls_transport_ptr ptr, const void *buf, size_t size)
{
	GIOChannel *parent = ptr;
	GIOStatus status;
	gint nr;
	status = g_io_channel_write_chars(parent, buf, size, &nr, NULL);
	
	switch (status) {
	case G_IO_STATUS_ERROR:
	case G_IO_STATUS_EOF:
		return -1;
	default:
		return nr;
	}
}
static GIOStatus g_io_gnutls_read(GIOChannel *handle, gchar *buf, guint len, guint *ret, GError **gerr)
{
	GIOTLSChannel *chan = (GIOTLSChannel *)handle;
	gint err;
	*ret = 0;

	if(!chan->secure) {
		err = gnutls_handshake(chan->session);
		if(err < 0) {
			log_global("gnutls", LOG_ERROR, "TLS Handshake failed");
			return g_io_gnutls_error(err);
		}
		chan->secure = 1;
	}

	err = gnutls_record_recv(chan->session, buf, len);
	if(err == 0) return G_IO_STATUS_EOF;
	if(err < 0) return g_io_gnutls_error(err);
	*ret = err;
	return G_IO_STATUS_NORMAL;
}

static GIOStatus g_io_gnutls_write(GIOChannel *handle, const gchar *buf, gsize len, gsize *ret, GError **gerr)
{
	GIOTLSChannel *chan = (GIOTLSChannel *)handle;
	gint err;

	if(!chan->secure) 
	{
		err = gnutls_handshake(chan->session);
		if(err < 0) {
			log_global("gnutls", LOG_ERROR, "TLS Handshake failed");
			return g_io_gnutls_error(err);
		}
		chan->secure = 1;
	}

	*ret = 0;

	err = gnutls_record_send(chan->session, (const char *)buf, len);
	if(err == 0) return G_IO_STATUS_EOF;
	if(err < 0) return g_io_gnutls_error(err);
	*ret = err;
	return G_IO_STATUS_NORMAL;
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

	return chan->giochan->funcs->io_create_watch(chan->giochan, cond);
}

static GIOStatus g_io_gnutls_set_flags(GIOChannel *handle, GIOFlags flags, GError **gerr)
{
    GIOTLSChannel *chan = (GIOTLSChannel *)handle;

    return chan->giochan->funcs->io_set_flags(chan->giochan, flags, gerr);
}

static GIOFlags g_io_gnutls_get_flags(GIOChannel *handle)
{
    GIOTLSChannel *chan = (GIOTLSChannel *)handle;

    return chan->giochan->funcs->io_get_flags(chan->giochan);
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

static gboolean fini_plugin(struct plugin *p)
{
	return TRUE;
}

static void load_config(struct global *global)
{
	char *cafile = NULL, *certf = NULL, *keyf = NULL;
	int err;

	keyf = g_key_file_get_string(global->config->keyfile, "ssl", "keyfile", NULL);
	certf = g_key_file_get_string(global->config->keyfile, "ssl", "certfile", NULL);
	cafile = g_key_file_get_string(global->config->keyfile, "ssl", "cafile", NULL);

	if(!certf) {
		certf = g_build_filename(global->config->config_dir, "cert.pem", NULL);
		if(!g_file_test(certf, G_FILE_TEST_EXISTS)) {
			free(certf);
			log_global("gnutls", LOG_ERROR, "No valid certificate set");
			return;
		}
	}

	if(!keyf) {
		keyf = g_build_filename(global->config->config_dir, "key.pem", NULL);
		if(!g_file_test(keyf, G_FILE_TEST_EXISTS)) {
			free(keyf);
			log_global("gnutls", LOG_ERROR, "No valid key set");
			return;
		}
	}

	if(!cafile) {
		cafile = g_build_filename(global->config->config_dir, "ca.pem", NULL);
		if(!g_file_test(cafile, G_FILE_TEST_EXISTS)) {
			free(cafile);
			cafile = NULL;
		} 
	}

	if (cafile) {
		err = gnutls_certificate_set_x509_trust_file(xcred, cafile, GNUTLS_X509_FMT_PEM);
		if(err < 0) {
			log_global("gnutls", LOG_ERROR, "Error setting x509 trust file: %s (file = %s)", gnutls_strerror(err), cafile);	
		}
	}

	if (!certf || !keyf) {
		log_global("gnutls", LOG_ERROR, "No certificate or key set!");
		return;
	}

	err = gnutls_certificate_set_x509_key_file(xcred, certf, keyf, GNUTLS_X509_FMT_PEM);
	if(err < 0) {
		log_global("gnutls", LOG_ERROR, "Error setting x509 key+cert files: %s (key = %s, cert = %s)", gnutls_strerror(err), keyf, certf);	
		return;
	}
}

static gboolean init_plugin(void)
{
	gnutls_dh_params dh_params;
	if(gnutls_global_init() < 0) {
		log_global("gnutls", LOG_ERROR, "gnutls global state initialization error");
		return FALSE;
	}

	gnutls_dh_params_init( &dh_params);
	gnutls_dh_params_generate2( dh_params, DH_BITS);

	/* X509 stuff */
	if (gnutls_certificate_allocate_credentials(&xcred) < 0) {	/* space for 2 certificates */
		log_global("gnutls", LOG_ERROR, "gnutls memory error");
		return FALSE;
	}

	gnutls_certificate_set_dh_params( xcred, dh_params);

	set_sslize_function (g_io_gnutls_get_iochannel);

	register_config_notify(load_config);

	return TRUE;
}

GIOChannel *g_io_gnutls_get_iochannel(GIOChannel *handle, gboolean server)
{
	GIOTLSChannel *chan;
	GIOChannel *gchan;

	g_return_val_if_fail(handle != NULL, NULL);
	
	chan = g_new0(GIOTLSChannel, 1);

	gnutls_init(&chan->session, server?GNUTLS_SERVER:GNUTLS_CLIENT);

	gnutls_set_default_priority(chan->session);
    gnutls_credentials_set(chan->session, GNUTLS_CRD_CERTIFICATE, xcred);
	gnutls_transport_set_ptr(chan->session, (gnutls_transport_ptr)handle);
	gnutls_transport_set_pull_function(chan->session, tls_pull);
	gnutls_transport_set_push_function(chan->session, tls_push);

	gnutls_certificate_server_set_request(chan->session, GNUTLS_CERT_REQUEST);
	gnutls_dh_set_prime_bits( chan->session, DH_BITS);
						   
	chan->secure = 0;
	chan->giochan = handle;
	chan->isserver = server;
	g_io_channel_ref(handle);

	gchan = (GIOChannel *)chan;
	gchan->funcs = &g_io_gnutls_channel_funcs;
	g_io_channel_init(gchan);
	
	return gchan;
}

struct plugin_ops plugin = {
	.name = "gnutls",
	.version = 0,
	.init = init_plugin,
};
