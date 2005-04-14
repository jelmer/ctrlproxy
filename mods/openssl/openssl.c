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

#include <ctrlproxy.h>
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

/* ssl i/o channel object */
typedef struct
{
	GIOChannel pad;
	gint fd;
	GIOChannel *giochan;
	SSL *ssl;
	X509 *cert;
	gboolean isserver;
} GIOSSLChannel;
	
static void irssi_ssl_free(GIOChannel *handle)
{
	GIOSSLChannel *chan = (GIOSSLChannel *)handle;
	g_io_channel_unref(chan->giochan);
	SSL_free(chan->ssl);
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

static GIOStatus irssi_ssl_cert_step(GIOSSLChannel *chan)
{
	gint err;
	switch(err = SSL_do_handshake(chan->ssl))
	{
		case 1:
			if(!(chan->cert = SSL_get_peer_certificate(chan->ssl)))
			{
				log_global("openssl", "SSL server supplied no certificate");
				return G_IO_STATUS_ERROR;
			}
			return G_IO_STATUS_NORMAL;
		default:
			if(SSL_get_error(chan->ssl, err) == SSL_ERROR_WANT_READ)
				return G_IO_STATUS_AGAIN;
			return irssi_ssl_errno(errno);
	}
	/*UNREACH*/
	return G_IO_STATUS_ERROR;
}

static GIOStatus irssi_ssl_read(GIOChannel *handle, gchar *buf, guint len, guint *ret, GError **gerr)
{
	GIOSSLChannel *chan = (GIOSSLChannel *)handle;
	gint err;
	
	if(chan->cert == NULL && !chan->isserver)
	{
		gint cert_err = irssi_ssl_cert_step(chan);
		if(cert_err != G_IO_STATUS_NORMAL)
			return cert_err;
	}
	
	err = SSL_read(chan->ssl, buf, len);
	if(err < 0)
	{
		*ret = 0;
		if(SSL_get_error(chan->ssl, err) == SSL_ERROR_WANT_READ)
			return G_IO_STATUS_AGAIN;
		return irssi_ssl_errno(errno);
	} else {
		*ret = err;
		if(err == 0) return G_IO_STATUS_EOF;
		return G_IO_STATUS_NORMAL;
	}
	/*UNREACH*/
	return G_IO_STATUS_ERROR;
}

static GIOStatus irssi_ssl_write(GIOChannel *handle, const gchar *buf, gsize len, gsize *ret, GError **gerr)
{
	GIOSSLChannel *chan = (GIOSSLChannel *)handle;
	gint err;

	if(chan->cert == NULL && !chan->isserver)
	{
		gint cert_err = irssi_ssl_cert_step(chan);
		if(cert_err != G_IO_STATUS_NORMAL)
			return cert_err;
	}

	err = SSL_write(chan->ssl, (const char *)buf, len);
	if(err < 0)
	{
		*ret = 0;
		if(SSL_get_error(chan->ssl, err) == SSL_ERROR_WANT_READ)
			return G_IO_STATUS_AGAIN;
		return irssi_ssl_errno(errno);
	}
	else
	{
		*ret = err;
		return G_IO_STATUS_NORMAL;
	}
	/*UNREACH*/
	return G_IO_STATUS_ERROR;
}

static GIOStatus irssi_ssl_seek(GIOChannel *handle, gint64 offset, GSeekType type, GError **gerr)
{
	GIOSSLChannel *chan = (GIOSSLChannel *)handle;
	GIOError e;
	e = g_io_channel_seek(chan->giochan, offset, type);
	return (e == G_IO_ERROR_NONE) ? G_IO_STATUS_NORMAL : G_IO_STATUS_ERROR;
}

static GIOStatus irssi_ssl_close(GIOChannel *handle, GError **gerr)
{
	GIOSSLChannel *chan = (GIOSSLChannel *)handle;
	g_io_channel_close(chan->giochan);

	return G_IO_STATUS_NORMAL;
}

static GSource *irssi_ssl_create_watch(GIOChannel *handle, GIOCondition cond)
{
	GIOSSLChannel *chan = (GIOSSLChannel *)handle;

	return chan->giochan->funcs->io_create_watch(handle, cond);
}

static GIOStatus irssi_ssl_set_flags(GIOChannel *handle, GIOFlags flags, GError **gerr)
{
    GIOSSLChannel *chan = (GIOSSLChannel *)handle;

    return chan->giochan->funcs->io_set_flags(handle, flags, gerr);
}

static GIOFlags irssi_ssl_get_flags(GIOChannel *handle)
{
    GIOSSLChannel *chan = (GIOSSLChannel *)handle;

    return chan->giochan->funcs->io_get_flags(handle);
}

static GIOFuncs irssi_ssl_channel_funcs = {
    irssi_ssl_read,
    irssi_ssl_write,
    irssi_ssl_seek,
    irssi_ssl_close,
    irssi_ssl_create_watch,
    irssi_ssl_free,
    irssi_ssl_set_flags,
    irssi_ssl_get_flags
};

GIOChannel *irssi_ssl_get_iochannel(GIOChannel *handle, gboolean server);
static gboolean irssi_ssl_set_files(const char *certf, const char *keyf);
static SSL_CTX *ssl_ctx = NULL;

const char name_plugin[] = "openssl";

gboolean fini_plugin(struct plugin *p) {
	return TRUE;
}

gboolean load_config(struct plugin *p, xmlNodePtr node)
{
	const char *keyf = NULL, *certf = NULL;
	xmlNodePtr cur;

	for (cur = node->children; cur; cur = cur->next)
	{
		if (cur->type != XML_ELEMENT_NODE) continue;
		
		if (!strcmp(cur->name, "keyfile")) keyf = xmlNodeGetContent(cur);
		if (!strcmp(cur->name, "certfile")) certf = xmlNodeGetContent(cur);
	}

	if(!certf) {
		certf = ctrlproxy_path("cert.pem");
		if(!g_file_test(certf, G_FILE_TEST_EXISTS)) {
			certf = NULL; 
		}
	}

	if(!keyf) {
		keyf = ctrlproxy_path("key.pem");
		if(!g_file_test(keyf, G_FILE_TEST_EXISTS)) { 
			keyf = NULL; 
		}
	}

	if(!irssi_ssl_set_files(certf, keyf)) {
		log_global("openssl", "Unable to set appropriate files");
		return FALSE;
	}
	
	return TRUE;
}

gboolean init_plugin(struct plugin *p)
{
	SSL_library_init();
	SSL_load_error_strings();
	
	ssl_ctx = SSL_CTX_new(SSLv23_method());
	if(!ssl_ctx)
	{
		log_global("openssl", "Initialization of the SSL library failed");
		return FALSE;
	}


	set_sslize_function (irssi_ssl_get_iochannel);

	return TRUE;
}

static gboolean irssi_ssl_set_files(const char *certf, const char *keyf)
{
	if(!ssl_ctx)
		return FALSE;

	if(SSL_CTX_use_certificate_file(ssl_ctx, certf, SSL_FILETYPE_PEM) <= 0) {
		log_global("openssl", "Can't set SSL certificate file %s!", certf);
		return FALSE;
	}

	if(SSL_CTX_use_PrivateKey_file(ssl_ctx, keyf, SSL_FILETYPE_PEM) <= 0) {
		log_global("openssl", "Can't set SSL private key file %s!", keyf);
		return FALSE;
	}

	if(!SSL_CTX_check_private_key(ssl_ctx)) {
		log_global("openssl", "Private key does not match the certificate public key!");
		return FALSE;
	}

	log_global("openssl", "Using SSL certificate from %s and SSL key from %s", certf, keyf);

	return TRUE;
}

GIOChannel *irssi_ssl_get_iochannel(GIOChannel *handle, gboolean server)
{
	GIOSSLChannel *chan;
	GIOChannel *gchan;
	int err, fd;
	SSL *ssl;
	X509 *cert = NULL;

	if(!handle) {
		log_global("openssl", "Invalid handle");
		return NULL;
	}
	
	if(!ssl_ctx) {
		log_global("openssl", "No valid openssl context available");
		return NULL;
	}

	if(!(fd = g_io_channel_unix_get_fd(handle))) {
		log_global("openssl", "Unable to get file descriptor");
		return NULL;
	}

	if((ssl = SSL_new(ssl_ctx)) == 0)
	{
		log_global("openssl", "Failed to allocate SSL structure");
		return NULL;
	}

	if(SSL_set_fd(ssl, fd) == 0)
	{
		log_global("openssl", "Failed to associate socket to SSL stream");
		return NULL;
	}
	
	if(server) err = SSL_accept(ssl);
	else err = SSL_connect(ssl);
	
	if(err <= 0 && SSL_get_error(ssl, err) != SSL_ERROR_WANT_READ && 
	   			   SSL_get_error(ssl, err) != SSL_ERROR_WANT_WRITE)
	{
		char buf[120];
		ERR_error_string(SSL_get_error(ssl, err), buf);
		log_global("openssl", "%s", buf);
		return NULL;
	}
	else if(!(cert = SSL_get_peer_certificate(ssl)))
	{
		if(!server)
		{
			log_global("openssl", "SSL %s supplied no certificate", server?"client":"server");
			return NULL;
		}
	}
	else
		X509_free(cert);

	chan = g_new0(GIOSSLChannel, 1);
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
