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
};

static gboolean socks_reply(GIOChannel *ioc, guint8 err, guint8 atyp, guint8 data_len, guint8 *data, guint16 port)
{
	guint8 *header = g_new0(guint8, 7 + data_len);
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
	return socks_reply(ioc, err, ATYP_FQDN, 1, &data, 0);
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
	guint8 header[2];
	gsize read;
	GIOStatus status;
	guint8 uname[0x100], pass[0x100];

	status = g_io_channel_read_chars(cl->connection, header, 2, &read, NULL);
	if (status != G_IO_STATUS_NORMAL) {
		return FALSE;
	}

	if (header[0] != SOCKS_VERSION && header[0] != 0x1) {
		log_global("socks", "Client suddenly changed socks uname/pwd version to %x", header[0]);
	 	return socks_error(cl->connection, REP_GENERAL_FAILURE);
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
		log_global("socks", "Password mismatch for user %s", uname);
		return FALSE;
	}
}

static struct network *socks_map_network_fqdn(const char *hostname, guint16 port)
{
	return find_network_by_hostname(hostname, port, TRUE);
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
		guint8 header[2];
		guint8 methods[0x100];
		gsize read;
		status = g_io_channel_read_chars(ioc, header, 2, &read, NULL);
		if (status != G_IO_STATUS_NORMAL) {
			return FALSE;
		}

		if (header[0] != SOCKS_VERSION) 
		{
			log_global("socks", "Ignoring client with socks version %d", header[0]);
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
			log_global("socks", "Refused client because no valid method was available");
			return FALSE;
		}

		log_global("socks", "Accepted socks client authenticating using %s", cl->method->name);

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
			log_global("socks", "Client suddenly changed socks version to %x", header[0]);
		 	return socks_error(ioc, REP_GENERAL_FAILURE);
		}

		if (header[1] != CMD_CONNECT) {
			log_global("socks", "Client used unknown command %x", header[1]);
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
					struct network *result;
					
					status = g_io_channel_read_chars(ioc, header, 1, &read, NULL);
					status = g_io_channel_read_chars(ioc, hostname, header[0], &read, NULL);
					hostname[header[0]] = '\0';

					status = g_io_channel_read_chars(ioc, header, 2, &read, NULL);
					port = ntohs(*(guint16 *)header);

					log_global("socks", "Request to connect to %s:%d", hostname, port);

					result = socks_map_network_fqdn(hostname, port);

					if (!result) {
						log_global("socks", "Unable to return network matching %s:%d", hostname, port);
						return socks_error(ioc, REP_NET_UNREACHABLE);
					} 

					if (result->connection.state == NETWORK_CONNECTION_STATE_NOT_CONNECTED && 
						!connect_network(result)) {
						log_network("socks", result, "Unable to connect");
						return socks_error(ioc, REP_NET_UNREACHABLE);
					}

					if (result->config->type == NETWORK_TCP) {
						struct sockaddr_in6 *name6; 
						struct sockaddr_in *name4; 
						int atyp, len, port;
						guint8 *data;

						name6 = (struct sockaddr_in6 *)result->connection.data.tcp.local_name;
						name4 = (struct sockaddr_in *)result->connection.data.tcp.local_name;

						if (name4->sin_family == AF_INET) {
							atyp = ATYP_IPV4;
							data = (guint8 *)&name4->sin_addr;
							len = 4;
							port = name4->sin_port;
						} else if (name6->sin6_family == AF_INET6) {
							atyp = ATYP_IPV6;
							data = (guint8 *)&name6->sin6_addr;
							len = 16;
							port = name6->sin6_port;
						} else {
							log_network("socks", result, "Unable to obtain local address for connection to server");
							return socks_error(ioc, REP_NET_UNREACHABLE);
						}
							
						socks_reply(ioc, REP_OK, atyp, len, data, port); 
						
					} else {
						char *data = g_strdup("xlocalhost");
						data[0] = strlen(data+1);
						
						socks_reply(ioc, REP_OK, ATYP_FQDN, data[0]+1, data, 1025);
					}

					new_client(result, ioc, NULL);

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
	cl->clientname_len = sizeof(struct sockaddr_in6);
	cl->clientname = g_malloc(cl->clientname_len);
	
	ns = accept(g_io_channel_unix_get_fd(ioc), cl->clientname, &cl->clientname_len);
	if (!ns) {
		g_free(cl->clientname);
		g_free(cl);
		log_global("socks", "Unable to accept connection");
		return TRUE;
	}
	
	cl->connection = g_io_channel_unix_new(ns);
	g_io_channel_set_close_on_unref(cl->connection, TRUE);
	cl->state = STATE_NEW;
	g_io_channel_set_encoding(cl->connection, NULL, NULL);
	cl->watch_id = g_io_add_watch(cl->connection, G_IO_IN | G_IO_HUP, handle_client_data, cl);
	g_io_channel_unref(cl->connection);

	pending_clients = g_list_append(pending_clients, cl);
	
	return TRUE;
}

/* Configure port number to listen on */
static gboolean fini_plugin(struct plugin *p)
{
	GList *gl;
	for (gl = pending_clients; gl; gl = gl->next) {
		struct socks_client *sc = gl->data;

		g_source_remove(sc->watch_id);

		g_free(sc->clientname);
		g_free(sc);
	}

	g_list_free(pending_clients);

	/* Close port */
	g_source_remove(server_channel_in);
	return TRUE;
}

static gboolean save_config(struct plugin *p, xmlNodePtr node)
{
	GList *gl;
	
	for (gl = allow_rules; gl ; gl = gl->next) {
		struct allow_rule *r = gl->data;
		xmlNodePtr n = xmlNewNode(NULL, "allow");
		if (r->username) xmlSetProp(n, "username", r->username);
		if (r->password) xmlSetProp(n, "password", r->password);

		xmlAddChild(node, n);
	}

	/* FIXME: Set port */

	return TRUE;
}

static gboolean load_config(struct plugin *p, xmlNodePtr conf)
{
	int sock;
	const int on = 1;
	struct sockaddr_in addr;
	guint16 port;
	xmlNodePtr cur;

	for (cur = conf->children; cur; cur = cur->next)
	{
		if (cur->type != XML_ELEMENT_NODE) continue;

		if (!strcmp(cur->name, "allow")) {
			struct allow_rule *r = g_new0(struct allow_rule, 1);
			
			r->username = xmlGetProp(cur, "username");
			r->password = xmlGetProp(cur, "password");

			allow_rules = g_list_append(allow_rules, r);
		}
	}

	port = DEFAULT_SOCKS_PORT;

	if (xmlHasProp(conf, "port")) {
		char *tmp = xmlGetProp(conf, "port");
		port = atoi(tmp);
		xmlFree(tmp);
	}

	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		log_global("socks", "error creating socket: %s", strerror(errno));
		return FALSE;
	}


    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind (sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		log_global("socks", "Unable to bind to port %d: %s", port, strerror(errno));
		return FALSE;
	}

	if (listen(sock, 5) < 0) {
		log_global("socks", "error listening on socket: %s", strerror(errno));
		return FALSE;
	}

	server_channel = g_io_channel_unix_new(sock);
	g_io_channel_set_close_on_unref(server_channel, TRUE);

	if (!server_channel) {
		log_global("socks", "Unable to create GIOChannel for server socket");
		return FALSE;
	}

	server_channel_in = g_io_add_watch(server_channel, G_IO_IN, handle_new_client, NULL);
	g_io_channel_unref(server_channel);

	log_global("socks", "Listening for SOCKS connections on port %d", port);

	return TRUE;
}

static gboolean init_plugin(struct plugin *p)
{
		return TRUE;
}

struct plugin_ops plugin = {
	.name = "socks",
	.version = 0,
	.init = init_plugin,
	.fini = fini_plugin,
	.load_config = load_config,
	.save_config = save_config
};
