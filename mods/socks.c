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
 *  - ipv6 support (listen)
 *  - support for ipv4 and ipv6 atyp's
 *  - support for gssapi method
 *  - support for ident method
 */

#include "ctrlproxy.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <glib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

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
static int server_channel_in = -1;
enum socks_state { STATE_NEW = 0, STATE_AUTH, STATE_NORMAL };

struct allow_rule {
	const char *username;
	const char *password;
};

static GList *allow_rules = NULL;
static GList *pending_clients = NULL;

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
	struct sockaddr *clientname;
	socklen_t clientname_len;
	struct global *global;
};

static gboolean socks_reply(GIOChannel *ioc, guint8 err, guint8 atyp, guint8 data_len, gchar *data, guint16 port)
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

	status = g_io_channel_write_chars(ioc, header, 6 + data_len, &read, NULL);

	g_free(header);

	g_io_channel_flush(ioc, NULL);

	return (err == REP_OK);
}

static gboolean socks_error(GIOChannel *ioc, guint8 err)
{
	guint8 data = 0x0;
	return socks_reply(ioc, err, ATYP_FQDN, 1, (gchar *)&data, 0);
}

static gboolean anon_acceptable(struct socks_client *cl)
{
	return FALSE; /* Don't allow anonymous connects */
}

static gboolean pass_acceptable(struct socks_client *cl)
{
	return TRUE; /* FIXME: Check whether there is a password specified */
}

static gboolean pass_handle_data(struct socks_client *cl)
{
	GList *gl;
	gchar header[2];
	gsize read;
	GIOStatus status;
	gchar uname[0x100], pass[0x100];

	status = g_io_channel_read_chars(cl->connection, header, 2, &read, NULL);
	if (status != G_IO_STATUS_NORMAL) {
		return FALSE;
	}

	if (header[0] != SOCKS_VERSION && header[0] != 0x1) {
		log_global(LOG_WARNING, "Client suddenly changed socks uname/pwd version to %x", header[0]);
	 	return socks_error(cl->connection, REP_GENERAL_FAILURE);
	}

	status = g_io_channel_read_chars(cl->connection, uname, header[1], &read, NULL);
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

	header[0] = 0x1;
	header[1] = 0x0; /* set to non-zero if invalid */

	for (gl = allow_rules; gl; gl = gl->next)
	{
		struct allow_rule *r = gl->data;

		if (!r->password || !r->username) 
			continue;

		if (strcmp(r->username, uname)) 
			continue;

		if (strcmp(r->password, pass))
			continue;
		break;
	}

	header[1] = (gl == NULL);

	status = g_io_channel_write_chars(cl->connection, header, 2, &read, NULL);
	if (status != G_IO_STATUS_NORMAL) {
		return FALSE;
	} 

	g_io_channel_flush(cl->connection, NULL);

	if (header[1] == 0x0) {
		cl->state = STATE_NORMAL;		
		return TRUE;
	} else {
		log_global(LOG_WARNING, "Password mismatch for user %s", uname);
		return FALSE;
	}
}

static struct network *socks_map_network_fqdn(struct global *global, const char *hostname, guint16 port)
{
	return find_network_by_hostname(global, hostname, port, TRUE);
}

struct socks_method {
	gint id;
	const char *name;
	gboolean (*acceptable) (struct socks_client *cl);
	gboolean (*handle_data) (struct socks_client *cl);
} socks_methods[] = {
	{ SOCKS_METHOD_NOAUTH, "none", anon_acceptable, NULL },
	{ SOCKS_METHOD_GSSAPI, "gssapi", NULL, NULL },
	{ SOCKS_METHOD_USERNAME_PW, "username/password", pass_acceptable, pass_handle_data },
	{ -1, NULL, NULL }
};

static gboolean handle_client_data (GIOChannel *ioc, GIOCondition o, gpointer data)
{
	struct socks_client *cl = data;
	GIOStatus status;
	int i;

	if (o == G_IO_HUP) {
		pending_clients = g_list_remove(pending_clients, cl);
		g_free(cl->clientname);
		g_free(cl);
		return FALSE;
	}

	if (cl->state == STATE_NEW) {
		gchar header[2];
		gchar methods[0x100];
		gsize read;
		status = g_io_channel_read_chars(ioc, header, 2, &read, NULL);
		if (status != G_IO_STATUS_NORMAL) {
			return FALSE;
		}

		if (header[0] != SOCKS_VERSION) 
		{
			log_global(LOG_WARNING, "Ignoring client with socks version %d", header[0]);
			return FALSE;
		}

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
			log_global(LOG_WARNING, "Refused client because no valid method was available");
			return FALSE;
		}

		log_global(LOG_INFO, "Accepted socks client authenticating using %s", cl->method->name);

		if (!cl->method->handle_data) {
			cl->state = STATE_NORMAL;
		} else {
			cl->state = STATE_AUTH;
		}
	} else if (cl->state == STATE_AUTH) {
		return cl->method->handle_data(cl);
	} else if (cl->state == STATE_NORMAL) {
		gchar header[4];
		gsize read;

		status = g_io_channel_read_chars(ioc, header, 4, &read, NULL);
		if (status != G_IO_STATUS_NORMAL) {
			return FALSE;
		}

		if (header[0] != SOCKS_VERSION) {
			log_global(LOG_WARNING, "Client suddenly changed socks version to %x", header[0]);
		 	return socks_error(ioc, REP_GENERAL_FAILURE);
		}

		if (header[1] != CMD_CONNECT) {
			log_global(LOG_WARNING, "Client used unknown command %x", header[1]);
			return socks_error(ioc, REP_CMD_NOT_SUPPORTED);
		}

		/* header[2] is reserved */
	
		switch (header[3]) {
			case ATYP_IPV4: 
				return socks_error(ioc, REP_ATYP_NOT_SUPPORTED);

			case ATYP_IPV6:
				return socks_error(ioc, REP_ATYP_NOT_SUPPORTED);

			case ATYP_FQDN:
				{
					char hostname[0x100];
					guint16 port;
					char *desc;
					struct network *result;
					
					status = g_io_channel_read_chars(ioc, header, 1, &read, NULL);
					status = g_io_channel_read_chars(ioc, hostname, header[0], &read, NULL);
					hostname[(guint8)header[0]] = '\0';

					status = g_io_channel_read_chars(ioc, header, 2, &read, NULL);
					port = ntohs(*(guint16 *)header);

					log_global(LOG_INFO, "Request to connect to %s:%d", hostname, port);

					result = socks_map_network_fqdn(cl->global, hostname, port);

					if (!result) {
						log_global(LOG_WARNING, "Unable to return network matching %s:%d", hostname, port);
						return socks_error(ioc, REP_NET_UNREACHABLE);
					} 

					if (result->connection.state == NETWORK_CONNECTION_STATE_NOT_CONNECTED && 
						!connect_network(result)) {
						log_network(LOG_ERROR, result, "Unable to connect");
						return socks_error(ioc, REP_NET_UNREACHABLE);
					}

					if (result->config->type == NETWORK_TCP) {
						struct sockaddr_in6 *name6; 
						struct sockaddr_in *name4; 
						int atyp, len, port;
						gchar *data;

						name6 = (struct sockaddr_in6 *)result->connection.data.tcp.local_name;
						name4 = (struct sockaddr_in *)result->connection.data.tcp.local_name;

						if (name4->sin_family == AF_INET) {
							atyp = ATYP_IPV4;
							data = (gchar *)&name4->sin_addr;
							len = 4;
							port = name4->sin_port;
						} else if (name6->sin6_family == AF_INET6) {
							atyp = ATYP_IPV6;
							data = (gchar *)&name6->sin6_addr;
							len = 16;
							port = name6->sin6_port;
						} else {
							log_network(LOG_ERROR, result, "Unable to obtain local address for connection to server");
							return socks_error(ioc, REP_NET_UNREACHABLE);
						}
							
						socks_reply(ioc, REP_OK, atyp, len, data, port); 
						
					} else {
						gchar *data = g_strdup("xlocalhost");
						data[0] = strlen(data+1);
						
						socks_reply(ioc, REP_OK, ATYP_FQDN, data[0]+1, data, 1025);
					}

					desc = g_io_channel_ip_get_description(ioc);
					client_init(result, ioc, desc);
					g_free(desc);

					pending_clients = g_list_remove(pending_clients, cl);

					g_free(cl->clientname);
					g_free(cl);

					return FALSE;
				}
			default:
				return socks_error(ioc, REP_ATYP_NOT_SUPPORTED);

		}
	}
	
	return TRUE;
}

static gboolean handle_new_client (GIOChannel *ioc, GIOCondition o, gpointer data)
{
	/* Spawn off new client */
	struct socks_client *cl;
	int ns;
	
	cl = g_new0(struct socks_client, 1);
	cl->global = data;
	cl->clientname_len = sizeof(struct sockaddr_in6);
	cl->clientname = g_malloc(cl->clientname_len);
	
	ns = accept(g_io_channel_unix_get_fd(ioc), cl->clientname, &cl->clientname_len);
	if (!ns) {
		g_free(cl->clientname);
		g_free(cl);
		log_global(LOG_ERROR, "Unable to accept connection");
		return TRUE;
	}
	
	cl->connection = g_io_channel_unix_new(ns);
	g_io_channel_set_close_on_unref(cl->connection, TRUE);
	cl->state = STATE_NEW;
	g_io_channel_set_encoding(cl->connection, NULL, NULL);
	g_io_channel_set_flags(cl->connection, G_IO_FLAG_NONBLOCK, NULL);
	cl->watch_id = g_io_add_watch(cl->connection, G_IO_IN | G_IO_HUP, handle_client_data, cl);
	g_io_channel_unref(cl->connection);

	pending_clients = g_list_append(pending_clients, cl);
	
	return TRUE;
}

void kill_pending_client(struct socks_client *sc)
{
	g_source_remove(sc->watch_id);

	g_free(sc->clientname);
	g_free(sc);

	pending_clients = g_list_remove(pending_clients, sc);
}

static void fini_plugin(void)
{
	while (pending_clients) {
		struct socks_client *sc = pending_clients->data;
		kill_pending_client(sc);
	}

	/* Close port */
	if (server_channel_in != -1)
		g_source_remove(server_channel_in);
}

static void load_config(struct global *global)
{
	int sock;
	const int on = 1;
	struct sockaddr_in addr;
	guint16 port;
	GKeyFile *kf = global->config->keyfile;
	gsize size, i;
	char **allows;

	allows = g_key_file_get_string_list(kf, "socks", "allow", &size, NULL);

	if (allows == NULL)
		return;

	for (i = 0; i < size; i++) {
		struct allow_rule *r = g_new0(struct allow_rule, 1);
		char **parts = g_strsplit(allows[i], ":", 2);
					
		r->username = parts[0];
		r->password = parts[1];

		g_free(parts);
		allow_rules = g_list_append(allow_rules, r);
	}

	g_strfreev(allows);

	if (g_key_file_has_key(kf, "socks", "port", NULL)) 
		port = g_key_file_get_integer(kf, "socks", "port", NULL);
	else 
		port = DEFAULT_SOCKS_PORT;

	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		log_global(LOG_ERROR, "error creating socket: %s", strerror(errno));
		return;
	}

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind (sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		log_global(LOG_ERROR, "Unable to bind to port %d: %s", port, strerror(errno));
		return;
	}

	if (listen(sock, 5) < 0) {
		log_global(LOG_ERROR, "error listening on socket: %s", strerror(errno));
		return;
	}

	server_channel = g_io_channel_unix_new(sock);
	g_io_channel_set_close_on_unref(server_channel, TRUE);

	if (!server_channel) {
		log_global(LOG_ERROR, "Unable to create GIOChannel for server socket");
		return;
	}

	server_channel_in = g_io_add_watch(server_channel, G_IO_IN, handle_new_client, global);
	g_io_channel_unref(server_channel);

	log_global(LOG_INFO, "Listening for SOCKS connections on port %d", port);
}

static gboolean init_plugin(void)
{
	register_load_config_notify(load_config);
	atexit(fini_plugin);
	return TRUE;
}

struct plugin_ops plugin = {
	.name = "socks",
	.version = 0,
	.init = init_plugin,
};
