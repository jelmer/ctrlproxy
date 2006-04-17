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
#include "ctrlproxy.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <nspr.h>
#include <private/pprio.h>
#include <prio.h>
#include <nss.h>
#include <ssl.h>
#include <sslerr.h>
#include <sslproto.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "nss"

static GIOStatus g_io_nss_error(int e) { return G_IO_STATUS_ERROR; }

typedef struct
{
	GIOChannel pad;
	gint fd;
	GIOChannel *giochan;
	PRFileDesc *session;
	gboolean isserver;
	CERTCertificate *cert;
	char secure;
} GIONSSChannel;

static void g_io_nss_free(GIOChannel *handle)
{
	GIONSSChannel *chan = (GIONSSChannel *)handle;
	g_io_channel_unref(chan->giochan);
	PR_Close(chan->session);
	g_free(chan);
}

static GIOStatus g_io_nss_read(GIOChannel *handle, gchar *buf, guint len, guint *ret, GError **gerr)
{
	GIONSSChannel *chan = (GIONSSChannel *)handle;
	gint err;
	*ret = 0;

	err = PR_Read(chan->session, buf, len);
	if(err == 0) return G_IO_STATUS_EOF;
	if(err < 0) return g_io_nss_error(err);
	*ret = err;
	return G_IO_STATUS_NORMAL;
}

static GIOStatus g_io_nss_write(GIOChannel *handle, const gchar *buf, gsize len, gsize *ret, GError **gerr)
{
	GIONSSChannel *chan = (GIONSSChannel *)handle;
	gint err;

	err = PR_Write(chan->session, buf, len);
	if(err == 0) return G_IO_STATUS_EOF;
	if(err < 0) return g_io_nss_error(err);
	*ret = err;
	return G_IO_STATUS_NORMAL;
}

static GIOStatus g_io_nss_seek(GIOChannel *handle, gint64 offset, GSeekType type, GError **gerr)
{
	GIONSSChannel *chan = (GIONSSChannel *)handle;
	GIOError e;
	e = g_io_channel_seek(chan->giochan, offset, type);
	return (e == G_IO_ERROR_NONE) ? G_IO_STATUS_NORMAL : G_IO_STATUS_ERROR;
}

static GIOStatus g_io_nss_close(GIOChannel *handle, GError **gerr)
{
	GIONSSChannel *chan = (GIONSSChannel *)handle;
	PR_Close(chan->session);
	g_io_channel_close(chan->giochan);
	return G_IO_STATUS_NORMAL;
}

static GSource *g_io_nss_create_watch(GIOChannel *handle, GIOCondition cond)
{
	GIONSSChannel *chan = (GIONSSChannel *)handle;

	return chan->giochan->funcs->io_create_watch(handle, cond);
}

static GIOStatus g_io_nss_set_flags(GIOChannel *handle, GIOFlags flags, GError **gerr)
{
    GIONSSChannel *chan = (GIONSSChannel *)handle;

    return chan->giochan->funcs->io_set_flags(handle, flags, gerr);
}

static GIOFlags g_io_nss_get_flags(GIOChannel *handle)
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

GIOChannel *g_io_nss_get_iochannel(GIOChannel *handle, gboolean server);

static PRDescIdentity _identity;

static gboolean init_plugin(void)
{   
	PR_Init(PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);
	NSS_NoDB_Init(NULL);
	NSS_SetDomesticPolicy();
	_identity = PR_GetUniqueIdentity("CtrlProxy");

	set_sslize_function (g_io_nss_get_iochannel);
	return TRUE;
}

GIOChannel *g_io_nss_get_iochannel(GIOChannel *handle, gboolean server)
{
	GIONSSChannel *chan;
	GIOChannel *gchan;
	int fd;

	g_return_val_if_fail(handle != NULL, NULL);
	
	if(!(fd = g_io_channel_unix_get_fd(handle)))
		return NULL;

	chan = g_new0(GIONSSChannel, 1);

	chan->session = SSL_ImportFD(NULL, PR_ImportTCPSocket(fd));
	if(chan->session == NULL) {
		return NULL;
	}
	SSL_OptionSet(chan->session, SSL_SECURITY, PR_TRUE);
	SSL_OptionSet(chan->session, server?SSL_HANDSHAKE_AS_SERVER:SSL_HANDSHAKE_AS_CLIENT, PR_TRUE);
	SSL_ResetHandshake(chan->session, PR_FALSE);

	chan->fd = fd;
	chan->secure = 0;
	chan->giochan = handle;
	chan->isserver = server;
	g_io_channel_ref(handle);

	gchan = (GIOChannel *)chan;
	gchan->funcs = &g_io_nss_channel_funcs;
	g_io_channel_init(gchan);
	
	return gchan;
}

struct plugin_ops plugin = {
	.name = "nss",
	.version = 0,
	.init = init_plugin,
};
