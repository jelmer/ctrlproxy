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

static gnutls_dh_params dh_params;
gboolean g_io_gnutls_set_files(char *certf, char *keyf, char *caf);
GIOChannel *g_io_gnutls_get_iochannel(GIOChannel *handle, gboolean server);

static int generate_dh_params(void) {

	/* Generate Diffie Hellman parameters - for use with DHE
	 * kx algorithms. These should be discarded and regenerated
	 * once a day, once a week or once a month. Depending on the
	 * security requirements.
	 */
	gnutls_dh_params_init( &dh_params);
	gnutls_dh_params_generate2( dh_params, DH_BITS);

	return 0;
}

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

static GIOStatus g_io_gnutls_error(gint e)
{
	switch(e)
	{
		case GNUTLS_E_INTERRUPTED:
		case GNUTLS_E_AGAIN:
			return G_IO_STATUS_AGAIN;
		default:
			log_global("gnutls", "TLS Error: %s", gnutls_strerror(e));
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

static GIOStatus g_io_gnutls_read(GIOChannel *handle, gchar *buf, guint len, guint *ret, GError **gerr)
{
	GIOTLSChannel *chan = (GIOTLSChannel *)handle;
	gint err;
	*ret = 0;

	if(!chan->secure) {
		err = gnutls_handshake(chan->session);
		if(err < 0) {
			log_global("gnutls", "TLS Handshake failed");
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
			log_global("gnutls", "TLS Handshake failed");
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

static gboolean fini_plugin(struct plugin *p)
{
	return TRUE;
}

#if 0

gboolean load_configuration() 
{
	xmlNodePtr cur;
	char *cafile = NULL, *certf = NULL, *keyf = NULL;

cur = config_instance_get_setting(p->config, "keyfile");
	if(cur) keyf = xmlNodeGetContent(cur);

	cur = config_instance_get_setting(p->config, "certfile");
	if(cur) certf = xmlNodeGetContent(cur);

	cur = config_instance_get_setting(p->config, "cafile");
	if(cur) cafile = xmlNodeGetContent(cur);

	if(!certf) {
		certf = ctrlproxy_path("cert.pem");
		if(!g_file_test(certf, G_FILE_TEST_EXISTS)) {
			free(certf);
			certf = NULL;
		}
	}

	if(!keyf) {
		keyf = ctrlproxy_path("key.pem");
		if(!g_file_test(keyf, G_FILE_TEST_EXISTS)) {
			free(keyf);
			keyf = NULL;
		}
	}

	if(!cafile) {
		cafile = ctrlproxy_path("ca.pem");
		if(!g_file_test(cafile, G_FILE_TEST_EXISTS)) {
			free(cafile);
			cafile = NULL;
		}
	}

	g_io_gnutls_set_files(certf, keyf, cafile);


}

#endif

static gboolean init_plugin(struct plugin *p)
{
	if(gnutls_global_init() < 0) {
		log_global("gnutls", "gnutls global state initialization error");
		return FALSE;
	}

	generate_dh_params();

	/* X509 stuff */
	if (gnutls_certificate_allocate_credentials(&xcred) < 0) {	/* space for 2 certificates */
		log_global("gnutls", "gnutls memory error");
		return FALSE;
	}

	gnutls_certificate_set_dh_params( xcred, dh_params);

	set_sslize_function (g_io_gnutls_get_iochannel);

	return TRUE;
}

gboolean g_io_gnutls_set_files(char *certf, char *keyf, char *caf)
{
	gint err;
	err = gnutls_certificate_set_x509_trust_file(xcred, caf, GNUTLS_X509_FMT_PEM);
	if(err < 0) {
		log_global("gnutls", "Error setting x509 trust file: %s (file = %s)", gnutls_strerror(err), caf);	
		return FALSE;
	}
	
	err = gnutls_certificate_set_x509_key_file(xcred, certf, keyf, GNUTLS_X509_FMT_PEM);
	if(err < 0) {
		log_global("gnutls", "Error setting x509 key+cert files: %s (key = %s, cert = %s)", gnutls_strerror(err), keyf, certf);	
		return FALSE;
	}

	return TRUE;
}

GIOChannel *g_io_gnutls_get_iochannel(GIOChannel *handle, gboolean server)
{
	GIOTLSChannel *chan;
	GIOChannel *gchan;
	static const int cert_type_priority[3] = { GNUTLS_CRT_X509, GNUTLS_CRT_OPENPGP, 0 };
	static const int cipher_priority[5] =
    { GNUTLS_CIPHER_AES_128_CBC, GNUTLS_CIPHER_3DES_CBC,
   GNUTLS_CIPHER_ARCFOUR_128, GNUTLS_CIPHER_ARCFOUR_40, 0 };
	int fd;

	g_return_val_if_fail(handle != NULL, NULL);
	
	if(!(fd = g_io_channel_unix_get_fd(handle)))
		return NULL;

	chan = g_new0(GIOTLSChannel, 1);

	gnutls_init(&chan->session, server?GNUTLS_SERVER:GNUTLS_CLIENT);

	gnutls_set_default_priority(chan->session);

	gnutls_cipher_set_priority(chan->session, cipher_priority);
	gnutls_certificate_type_set_priority(chan->session, cert_type_priority);
	gnutls_handshake_set_private_extensions(chan->session, 1);
    gnutls_credentials_set(chan->session, GNUTLS_CRD_CERTIFICATE, xcred);
	gnutls_transport_set_ptr(chan->session, (gnutls_transport_ptr)fd);

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

struct plugin_ops plugin = {
	.name = "gnutls",
	.version = 0,
	.init = init_plugin,
	.fini = fini_plugin,
};
