/* 
	ctrlproxy: A modular IRC proxy
	(c) 2005 Jelmer Vernooij <jelmer@nl.linux.org>
	SOCKS server (see RFC 1928, 1929 and 1961)

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
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/* TODO:
 *  - ipv6 support
 */

#define _GNU_SOURCE
#include "ctrlproxy.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <glib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "socks"

#define SOCKS_VERSION 0x05
#define DEFAULT_SOCKS_PORT 1080

#define SOCKS_METHOD_NOAUTH		  	0x00
#define SOCKS_METHOD_GSSAPI 	  	0x01
#define SOCKS_METHOD_USERNAME_PW  	0x02
#define SOCKS_METHOD_NOACCEPTABLE 	0xFF

#define ATYP_IPV4					0x01
#define ATYP_FQDN					0x03
#define ATYP_IPV6					0x04

#define CMD_CONNECT					0x01
#define CMD_BIND					0x02
#define CMD_UDP_ASSOCIATE			0x03

#define REP_OK						0x00
#define REP_GENERAL_FAILURE			0x01
#define REP_NOT_ALLOWED				0x02
#define REP_NET_UNREACHABLE			0x03
#define REP_HOST_UNREACHABLE		0x04
#define REP_CONN_REFUSED			0x05
#define REP_TTL_EXPIRED				0x06
#define REP_CMD_NOT_SUPPORTED		0x07
#define REP_ATYP_NOT_SUPPORTED		0x08

static GIOChannel *server_channel = NULL;
static int server_channel_in = 0;
enum socks_state { STATE_NEW = 0, STATE_AUTH, STATE_NORMAL };

struct socks_method;
struct socks_client
{
	GIOChannel *connection;
	const char *user;
	const char *password;
	gint watch_id;
	struct socks_method *method;
	enum socks_state state;
	void *method_data;
};

static void socks_error(GIOChannel *ioc, guint8 err)
{
	/* FIXME */
}

static gboolean anon_acceptable(struct socks_client *cl)
{
	return TRUE; /* FIXME: Check whether anonymous connects
	should be allowed */
}

static gboolean pass_acceptable(struct socks_client *cl)
{
	return TRUE; /* FIXME: Check whether there is a password specified */
}

static gboolean pass_handle_data(struct socks_client *cl)
{
	guint8 header[2];
	gsize read;
	GIOStatus status;
	guint8 uname[0x100], pass[0x100];

	status = g_io_channel_read_chars(cl->connection, header, 2, &read, NULL);
	if (status != G_IO_STATUS_NORMAL) {
		return FALSE;
	}

	if (header[0] != 0x1) {
	 	socks_error(cl->connection, REP_GENERAL_FAILURE);
		g_warning("Client suddenly changed socks version to %x", header[0]);
		return FALSE;
	}

	status = g_io_channel_read_chars(cl->connection, uname, header[1], &read, NULL);
	if (status != G_IO_STATUS_NORMAL) {
		return FALSE;
	}

	uname[header[1]] = '\0';

	status = g_io_channel_read_chars(cl->connection, header, 1, &read, NULL);
	if (status != G_IO_STATUS_NORMAL) {
		return FALSE;
	}

	status = g_io_channel_read_chars(cl->connection, pass, header[0], &read, NULL);
	if (status != G_IO_STATUS_NORMAL) {
		return FALSE;
	}

	pass[header[0]] = '\0';

	g_message("Client tried to log in with username %s, pass %s", uname, pass);

	header[0] = 0x1;
	header[1] = 0x0; /* FIXME: Verify password and set to non-zero if invalid */

	status = g_io_channel_write_chars(cl->connection, header, 2, &read, NULL);
	if (status != G_IO_STATUS_NORMAL) {
		return FALSE;
	} 

	g_io_channel_flush(cl->connection, NULL);

	return header[1] == 0x0;
}

struct socks_method {
	gint id;
	gboolean (*acceptable) (struct socks_client *cl);
	gboolean (*handle_data) (struct socks_client *cl);
} socks_methods[] = {
	{ SOCKS_METHOD_NOAUTH, anon_acceptable, NULL },
	{ SOCKS_METHOD_GSSAPI, NULL, NULL },
	{ SOCKS_METHOD_USERNAME_PW, pass_acceptable, pass_handle_data },
	{ -1, NULL, NULL }
};

static gboolean handle_client_data (GIOChannel *ioc, GIOCondition o, gpointer data)
{
	struct socks_client *cl = data;
	GIOStatus status;
	int i;

	if (cl->state == STATE_NEW) {
		guint8 header[2];
		guint8 methods[0x100];
		gsize read;
		status = g_io_channel_read_chars(ioc, header, 2, &read, NULL);
		if (status != G_IO_STATUS_NORMAL) {
			return FALSE;
		}

		if (header[0] != SOCKS_VERSION) 
		{
			g_warning("Ignoring client with socks version %d", header[0]);
			return FALSE;
		}

		g_message("New SOCKS client with version %d", header[0]);

		/* None by default */
		cl->method = NULL;

		status = g_io_channel_read_chars(ioc, methods, header[1], &read, NULL);
		if (status != G_IO_STATUS_NORMAL) {
			return FALSE;
		}
		for (i = 0; i < header[1]; i++) {
			int j;
			for (j = 0; socks_methods[j].id != -1; j++)
			{
				if (socks_methods[j].id == methods[i] && 
					socks_methods[j].acceptable &&
					socks_methods[j].acceptable(cl)) {
					cl->method = &socks_methods[j];
					break;
				}
			}
		}

		header[0] = SOCKS_VERSION;
		header[1] = cl->method?cl->method->id:SOCKS_METHOD_NOACCEPTABLE;

		status = g_io_channel_write_chars(ioc, header, 2, &read, NULL);
		if (status != G_IO_STATUS_NORMAL) {
			return FALSE;
		} 

		g_io_channel_flush(ioc, NULL);

		if (!cl->method) {
			g_warning("Refused client because no valid method was available");
			return FALSE;
		}

		g_message("Accepted authentication %x", cl->method->id);

		if (!cl->method->handle_data) {
			cl->state = STATE_NORMAL;
		} else {
			cl->state = STATE_AUTH;
		}
	} else if (cl->state == STATE_AUTH) {
		return cl->method->handle_data(cl);
	} else if (cl->state == STATE_NORMAL) {
		guint8 header[4];
		gsize read;

		status = g_io_channel_read_chars(ioc, header, 4, &read, NULL);
		if (status != G_IO_STATUS_NORMAL) {
			return FALSE;
		}

		if (header[0] != SOCKS_VERSION) {
		 	socks_error(ioc, REP_GENERAL_FAILURE);
			g_warning("Client suddenly changed socks version to %x", header[0]);
			return FALSE;
		}

		if (header[1] != CMD_CONNECT) {
			socks_error(ioc, REP_CMD_NOT_SUPPORTED);
			g_warning("Client used unknown command %x", header[1]);
			return FALSE;
		}

		/* FIXME */

	}
	
	return TRUE;
}

static gboolean handle_new_client (GIOChannel *ioc, GIOCondition o, gpointer data)
{
	/* Spawn off new client */
	struct socks_client *cl;
	int ns;
	struct sockaddr clientname;
	size_t size;
	
	size = sizeof(clientname);
	ns = accept(g_io_channel_unix_get_fd(ioc), &clientname, &size);
	if (!ns) {
		g_warning("Unable to accept connection");
		return TRUE;
	}
	
	cl = g_new0(struct socks_client, 1);
	cl->connection = g_io_channel_unix_new(ns);
	cl->state = STATE_NEW;
	g_io_channel_set_encoding(cl->connection, NULL, NULL);
	cl->watch_id = g_io_add_watch(cl->connection, G_IO_IN, handle_client_data, cl);
	g_io_channel_unref(cl->connection);
	
	return TRUE;
}

/* Configure port number to listen on */
gboolean fini_plugin(struct plugin *p)
{
	/* Close port */
	g_source_remove(server_channel_in);
	return TRUE;
}

const char name_plugin[] = "socks";

gboolean init_plugin(struct plugin *p)
{
	int sock;
	const int on = 1;
	struct sockaddr_in addr;
	guint16 port;

	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		g_warning( "error creating socket: %s", strerror(errno));
		return FALSE;
	}

	port = DEFAULT_SOCKS_PORT;

	/* FIXME: Get port from configuration */

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind (sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		g_warning("Unable to bind to port %d: %s", port, strerror(errno));
		return FALSE;
	}

	if (listen(sock, 5) < 0) {
		g_warning( "error listening on socket: %s", strerror(errno));
		return FALSE;
	}

	server_channel = g_io_channel_unix_new(sock);

	if (!server_channel) {
		g_warning("Unable to create GIOChannel for server socket");
		return FALSE;
	}

	server_channel_in = g_io_add_watch(server_channel, G_IO_IN, handle_new_client, NULL);
	g_io_channel_unref(server_channel);

	g_message("Listening for SOCKS connections on port %d", port);
	
	return TRUE;
}
