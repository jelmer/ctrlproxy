/*
	ctrlproxy: A modular IRC proxy
	ip: support for connecting and listening on ipv4 and ipv6
	(c) 2002-2003 Jelmer Vernooij <jelmer@nl.linux.org>

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

#define _GNU_SOURCE
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "ctrlproxy.h"
#ifdef _WIN32
#include <winsock.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/un.h>
#endif
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include "gettext.h"
#define _(s) gettext(s)

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "socket"

enum ssl_mode { SSL_MODE_NONE = 0, SSL_MODE_SERVER = 1, SSL_MODE_CLIENT = 2};

/* Prototypes from network-openssl.c */
GIOChannel *irssi_ssl_get_iochannel(GIOChannel *handle, gboolean server);
GIOChannel *g_io_gnutls_get_iochannel(GIOChannel *handle, gboolean server);
gboolean irssi_ssl_set_files(char *certf, char *keyf);
gboolean g_io_gnutls_set_files(char *certf, char *keyf, char *caf);
gboolean g_io_gnutls_fini();
gboolean g_io_gnutls_init();

static gboolean handle_in (GIOChannel *c, GIOCondition o, gpointer data);
static gboolean handle_disc (GIOChannel *c, GIOCondition o, gpointer data);

static int socket_tos = 0;

struct socket_data {
	GIOChannel *channel;
	guint disc_id;
	guint in_id;
};

static pid_t piped_child(char* const command[], int *f_in)
{
	pid_t pid;
	int sock[2];

	if(socketpair(PF_UNIX, SOCK_STREAM, AF_LOCAL, sock) == -1) {
		g_warning( "socketpair: %s", strerror(errno));
		return -1;
	}

	*f_in = sock[0];

	fcntl(sock[0], F_SETFL, O_NONBLOCK);

	pid = fork();
	if (pid == -1) {
		g_warning( "fork: %s", strerror(errno));
		return -1;
	}

	if (pid == 0) {
		close(0);
		close(1);
		close(2);
		close(sock[0]);

		dup2(sock[1], 0);
		dup2(sock[1], 1);
		execvp(command[0], command);
		g_warning( _("Failed to exec %s : %s"), command[0], strerror(errno));
		return -1;
	}

	close(sock[1]);

	return pid;
}

static gboolean handle_out (GIOChannel *ioc, GIOCondition o, gpointer data)
{
	struct transport_context *c = (struct transport_context *)data;

	g_assert(o == G_IO_OUT);

	/* only on outgoing connections */
	if(c->on_connect) c->on_connect(c, c->caller_data);

	return FALSE;
}

static void socket_to_iochannel(int sock, struct transport_context *c, enum ssl_mode ssl_mode)
{
	GIOChannel *ioc;
	struct socket_data *s = malloc(sizeof(struct socket_data));
	GError *error = NULL;
	ioc = g_io_channel_unix_new(sock);
#if defined(HAVE_GNUTLS_GNUTLS_H)
	if(ssl_mode != SSL_MODE_NONE) {
		GIOChannel *newioc;
		newioc = g_io_gnutls_get_iochannel(ioc, ssl_mode == SSL_MODE_SERVER);

		if(!newioc) g_warning(_("Can't convert socket to SSL"));
		else ioc = newioc;
	}
#elif defined(HAVE_OPENSSL_SSL_H)
	if(ssl_mode != SSL_MODE_NONE) {
		GIOChannel *newioc;
		newioc = irssi_ssl_get_iochannel(ioc, ssl_mode == SSL_MODE_SERVER);

		if(!newioc) g_warning(_("Can't convert socket to SSL"));
		else ioc = newioc;
	}
#endif
	
	g_io_channel_set_encoding(ioc, NULL, &error);
	if(error)g_error_free(error);

	g_io_channel_set_close_on_unref(ioc, TRUE);
	s->disc_id = g_io_add_watch(ioc, G_IO_IN, handle_in, (gpointer)c);
	s->in_id = g_io_add_watch(ioc, G_IO_HUP, handle_disc, (gpointer)c);
	
	g_io_add_watch(ioc, G_IO_OUT, handle_out, (gpointer)c);

	s->channel = ioc;
	c->data = s;
}

static int connect_pipe(struct transport_context *c)
{
	char *args[100];
	int i;
	int argc = 0;
	int sock;
	pid_t pid;
	xmlNodePtr cur = c->configuration->xmlChildrenNode;
	memset(args, 0, sizeof(char *) * 100);


	while(cur) {
		if(xmlIsBlankNode(cur) || !strcmp(cur->name, "comment")) {
			cur = cur->next;
			continue;
		}

		if(!strcmp(cur->name, "path")) args[0] = xmlNodeGetContent(cur);
		else if(!strcmp(cur->name, "arg")) args[++argc] = xmlNodeGetContent(cur);
		else g_warning( _("Unknown element %s"), cur->name);

		cur = cur->next;
	}

	xmlSetProp(c->configuration, "name", args[0]);

	args[++argc] = NULL;

	pid = piped_child(args, &sock);

	for(i = 0; i < argc; i++) xmlFree(args[i]);

	if(pid == -1) return -1;

	socket_to_iochannel(sock, c, SSL_MODE_NONE);

	return 0;
}

static gboolean handle_new_client(GIOChannel *c, GIOCondition o, gpointer data)
{
	struct sockaddr clientname;
	struct transport_context *newcontext, *oldcontext = (struct transport_context *)data;
	size_t size;
	int sock;
	char *ssl = NULL;

	g_assert(o == G_IO_IN);

	size = sizeof(clientname);
	sock = accept(g_io_channel_unix_get_fd(c), (struct sockaddr *)&clientname, &size);
	if(!sock) {
		g_warning( _("Can't accept connection!"));
		return FALSE;
	}
	g_message(_("New client %d"), sock);

	newcontext = malloc(sizeof(struct transport_context));
	memset(newcontext, 0, sizeof(struct transport_context));

	ssl = xmlGetProp(oldcontext->configuration, "ssl");
	socket_to_iochannel(sock, newcontext, (ssl && atoi(ssl))?SSL_MODE_SERVER:SSL_MODE_NONE);
	xmlFree(ssl);
	
	newcontext->functions = oldcontext->functions;
	newcontext->configuration = oldcontext->configuration;

	if(oldcontext->on_new_client) oldcontext->on_new_client(oldcontext, newcontext, oldcontext->caller_data);
	return TRUE;
}

static gboolean handle_disc (GIOChannel *ioc, GIOCondition o, gpointer data)
{
	struct transport_context *c = (struct transport_context *)data;

	if(c->on_disconnect)c->on_disconnect(c, c->caller_data);

	return TRUE;
}

static gboolean handle_in (GIOChannel *ioc, GIOCondition o, gpointer data)
{	
	struct transport_context *c = (struct transport_context *)data;
	GIOStatus status;
	char *ret;
	GError *error = NULL;

	g_assert(o == G_IO_IN);

	if(!(g_io_channel_get_flags(ioc) & G_IO_FLAG_IS_READABLE)) {
		g_warning(_("Channel is not readeable!"));
		return -1;
	}

	status = g_io_channel_read_line(ioc, &ret, NULL, NULL, &error);

	switch(status){
	case G_IO_STATUS_ERROR:
		g_warning( "%s", (error?error->message:_("Unknown error")));
		if(error)g_error_free(error);
		return TRUE;

	case G_IO_STATUS_EOF:
		if(error)g_error_free(error);
		if(c->on_disconnect)c->on_disconnect(c, c->caller_data);
		return FALSE;

	case G_IO_STATUS_AGAIN:
		if(error)g_error_free(error);
		return TRUE;

	case G_IO_STATUS_NORMAL: 
		if(c->on_receive)c->on_receive(c, ret, c->caller_data);
		free(ret);
		return TRUE;

	default: break;
	}
	g_assert(0);
	return TRUE;
}

static int connect_ip(struct transport_context *c)
{
	struct sockaddr_in name4;
	struct sockaddr_in6 name6;
	char *hostname = xmlGetProp(c->configuration, "host");
	char *aPort = xmlGetProp(c->configuration, "port");
	uint16_t port;
	struct hostent *hostinfo;
	int ret;
	int sock;
	char *tempname;
	char *ssl;
	char ipv6 = 0;
	int family = AF_INET;
	int domain = PF_INET;
	char *bind_addr;
	struct hostent *bind_host = NULL;


	if(!strcmp(c->functions->name, "ipv6")){
		ipv6 = 1;
		domain = PF_INET6;
		family = AF_INET6;
	}
	memset(&name6, 0, sizeof(name6));
	memset(&name4, 0, sizeof(name4));

	port = aPort?atoi(aPort):6667;
	if(aPort)xmlFree(aPort);

	g_assert(hostname);

	asprintf(&tempname, "%s:%d", hostname, port);

	xmlSetProp(c->configuration, "name", tempname);

	g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, _("Connecting to %s:%d"), hostname, port);

	sock = socket (domain, SOCK_STREAM, 0);

	if (sock < 0)
	{
		g_warning( _("(While connecting to server %s with %s): socket: %s"), tempname, strerror(errno), c->functions->name);
		xmlFree(hostname);
		free(tempname);
		return -1;
	}

	bind_addr = xmlGetProp(c->configuration, "bind");
	if(bind_addr)
		bind_host = gethostbyname2(bind_addr, family);

	ret = 0;
	if(ipv6) {
		name6.sin6_family = AF_INET6;
		if(bind_host) {
			memcpy((char *)&name6.sin6_addr, bind_host->h_addr, bind_host->h_length);
			ret = bind (sock, (struct sockaddr *) &name6, sizeof (name6));
		}
		name6.sin6_port = htons (port);
	} else {
		name4.sin_family = AF_INET;
		if(bind_host) {
			name4.sin_addr = *(struct in_addr *) bind_host->h_addr;
			ret = bind (sock, (struct sockaddr *) &name4, sizeof (name4));
		}
		name4.sin_port = htons (port);
	}

	xmlFree(bind_addr);

	if(ret < 0)
	{
		g_warning( "bind: %s", strerror(errno));
		xmlFree(hostname);
		return -1;
	}

	hostinfo = gethostbyname2 (hostname, family);

	if (hostinfo == NULL)
	{
		g_warning(_("Unknown host %s."), hostname);
		xmlFree(hostname);
		free(tempname);
		return -1;
	}
	if(socket_tos != 0)
        if (setsockopt(sock, IPPROTO_IP, IP_TOS, &socket_tos, sizeof(socket_tos)) < 0)
                g_warning( "setsockopt IP_TOS %d: %.100s:", socket_tos, strerror(errno));

	if(ipv6) {
		memcpy((char *)&name6.sin6_addr, hostinfo->h_addr, hostinfo->h_length);
		ret = connect(sock, (struct sockaddr *)&name6, sizeof(name6));
	} else {
		name4.sin_addr = *(struct in_addr *) hostinfo->h_addr;
		ret = connect(sock, (struct sockaddr *)&name4, sizeof(name4));
	}

	if (ret < 0 && errno != EINPROGRESS)
	{
		g_warning( _("(While connecting to server %s): connect: %s"), tempname, strerror(errno));
		xmlFree(hostname);
		free(tempname);
		return -1;
	}
	free(tempname);


	ssl = xmlGetProp(c->configuration, "ssl");
	socket_to_iochannel(sock, c, (ssl && atoi(ssl))?SSL_MODE_CLIENT:SSL_MODE_NONE);
	xmlFree(ssl);

	xmlFree(hostname);
	return 0;
}

static int close_socket(struct transport_context *c)
{
	struct socket_data *s;

	if(!c) return -1;

	s = (struct socket_data *)c->data;
	if(!s) return -1;

	g_source_remove(s->in_id);
	g_source_remove(s->disc_id);
	g_io_channel_unref(s->channel);

	free(c->data);

	c->data = NULL;
	return 0;
}

static int write_socket(struct transport_context *t, char *l)
{
	GError *error = NULL;
	struct socket_data *s = (struct socket_data *)t->data;

	if(!s->channel) {
		g_message(_("Trying to send line '%s' to socket that is not connected!"), l);
		return -1;
	}

	if(!(g_io_channel_get_flags(s->channel) & G_IO_FLAG_IS_WRITEABLE)) {
		g_warning(_("Channel is not writeable!"));
		return -1;
	}

	if(g_io_channel_write_chars(s->channel, l, -1, NULL, &error) == G_IO_STATUS_ERROR)
	{
		g_message(_("Can't send: %s"), (error?error->message:"g_io_channel_write_chars failed"));
		if(error)g_error_free(error);
		return -1;
	}

	g_assert(!error);
	g_io_channel_flush(s->channel, &error);

	if(error) {
		g_error_free(error);
		return -1;
	}

	return 0;
}

static int listen_ip(struct transport_context *c)
{
	static int client_port = 6667;
	char ipv6 = 0;
	struct sockaddr_in6 name6;
	struct sockaddr_in name4;
	struct socket_data *s;
	char *bind_addr = NULL;
	struct hostent *bind_host = NULL;
	const int on = 1;
	int port, sock;
	int domain = PF_INET, family = AF_INET;
	int ret;
	GIOChannel *gio;
	GError *error = NULL;

	if(!strcmp(c->functions->name, "ipv6")) {
		ipv6 = 1;
		domain = PF_INET6;
		family = AF_INET6;
	}

	if(xmlHasProp(c->configuration, "port")) {
		char *aport = xmlGetProp(c->configuration, "port");
		port = atoi(aport);
		xmlFree(aport);
	} else port = ++client_port;

	/* Create the socket. */
	sock = socket (domain, SOCK_STREAM, 0);
	if (sock < 0)
	{
		g_warning( "socket: %s", strerror(errno));
		return -1;
	}

	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	if(socket_tos != 0)
        if (setsockopt(sock, IPPROTO_IP, IP_TOS, &socket_tos, sizeof(socket_tos)) < 0)
                g_warning( "setsockopt IP_TOS %d: %.100s:", socket_tos, strerror(errno));

	bind_addr = xmlGetProp(c->configuration, "bind");
	if(bind_addr) bind_host = gethostbyname2(bind_addr, family);
	xmlFree(bind_addr);

	/* Give the socket a name. */
	if(ipv6) {
		memset(&name6, 0, sizeof(name6));
		name6.sin6_family = AF_INET6;
		name6.sin6_port = htons(port);
		if(bind_host) {
			memcpy((char *)&name6.sin6_addr, bind_host->h_addr, bind_host->h_length);
		} else {
			name6.sin6_addr = in6addr_any;
		}
		ret = bind (sock, (struct sockaddr *) &name6, sizeof (name6));
	} else {
		memset(&name4, 0, sizeof(name4));
		name4.sin_family = AF_INET;
		name4.sin_port = htons (port);
		if(bind_host) {
			name4.sin_addr = *(struct in_addr *) bind_host->h_addr;
		} else {
			name4.sin_addr.s_addr = htonl (INADDR_ANY);
		}
		ret = bind (sock, (struct sockaddr *) &name4, sizeof (name4));
	}

	if(ret < 0)
	{
		g_warning( "bind: %s", strerror(errno));
		return -1;
	}

	if(listen(sock, 5) < 0) {
		g_warning( _("Error trying to listen on port %d: %s"), port, strerror(errno));
		return -1;
	}

	g_log(G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE, _("Listening on port %d(socket %d)"), port, sock);

	gio = g_io_channel_unix_new(sock);

	g_io_channel_set_encoding(gio, NULL, &error);
	if(error)g_error_free(error);

	s = malloc(sizeof(struct socket_data));
	s->channel = gio;
	s->disc_id = -1;
	s->in_id = g_io_add_watch(gio, G_IO_IN, handle_new_client, c);

	c->data = s;

	return 0;
}

static int listen_pipe(struct transport_context *c)
{
	struct sockaddr_un name;
	int sock;
	char *f;
	struct socket_data *s;
	GIOChannel *gio;
	GError *error = NULL;

	if(xmlHasProp(c->configuration, "file")) {
		f = xmlGetProp(c->configuration, "file");
	} else {
		char *nname = xmlGetProp(c->configuration, "name");
		asprintf(&f, "%s/ctrlproxy-%s", getenv("TMPDIR")?getenv("TMPDIR"):"/tmp", nname?nname:"");
		xmlFree(nname);
	}

	/* Create the socket. */
	sock = socket (AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0)
	{
		g_warning("socket(%s): %s", f, strerror(errno));
		free(f);
		return -1;
	}

	name.sun_family = AF_UNIX;
	strcpy(name.sun_path, f);

	unlink(f);

	/* Give the socket a name. */
	if(bind (sock, (struct sockaddr *) &name, sizeof (name)) < 0)
	{
		g_warning("bind(%s): %s", f, strerror(errno));
		free(f);
		return -1;
	}

	if(listen(sock, 128) < 0) {
		g_warning(_("Error trying to listen on %s: %s"), f, strerror(errno));
		free(f);
		return -1;
	}

	g_message(_("Listening on socket at %s(%d)"), f, sock);

	gio = g_io_channel_unix_new(sock);

	g_io_channel_set_encoding(gio, NULL, &error);
	if(error)g_error_free(error);

	s = malloc(sizeof(struct socket_data));
	s->channel = gio;
	s->in_id = g_io_add_watch(gio, G_IO_IN, handle_new_client, c);
	s->disc_id = -1;

	c->data = s;
	free(f);

	return 0;
}

static struct transport_ops ipv4 = {
	"ipv4",
	connect_ip,
	listen_ip,
	write_socket,
	close_socket
};

static struct transport_ops ipv6 = {
	"ipv6",
	connect_ip,
	listen_ip,
	write_socket,
	close_socket
};

static struct transport_ops pipe_transport = {
	"pipe",
	connect_pipe,
	listen_pipe,
	write_socket,
	close_socket
};

gboolean fini_plugin(struct plugin *p) {
	g_io_gnutls_fini();
	return (unregister_transport("ipv4") &&
			unregister_transport("ipv6") &&
			unregister_transport("pipe"));
}

const char name_plugin[] = "socket";

gboolean init_plugin(struct plugin *p) {
	xmlNodePtr cur;
	char *cur2;
	char *certf = NULL, *keyf = NULL, *caf = NULL;;
	register_transport(&ipv4);
	register_transport(&ipv6);
	register_transport(&pipe_transport);

	cur = p->xmlConf->xmlChildrenNode;
	while(cur) {
		if(xmlIsBlankNode(cur) || !strcmp(cur->name, "comment")) { cur = cur->next; continue; }

		if(!strcmp(cur->name, "sslkeyfile")) keyf = xmlNodeGetContent(cur);
		else if(!strcmp(cur->name, "sslcertfile")) certf = xmlNodeGetContent(cur);
		else if(!strcmp(cur->name, "sslcafile")) caf = xmlNodeGetContent(cur);
		else if(!strcmp(cur->name, "tos") && xmlHasProp(cur, "port")) {
			cur2 = xmlGetProp(cur, "value");
			if(!strcasecmp(cur2,"Minimize-Delay")) {
				socket_tos = IPTOS_LOWDELAY;
			} else if(!strcasecmp(cur2,"Maximize-Throughput")) {
				socket_tos = IPTOS_THROUGHPUT;
			} else if(!strcasecmp(cur2,"Maximize-Reliability")) {
				socket_tos = IPTOS_RELIABILITY;
			} else if(!strcasecmp(cur2,"Minimize-Cost")) {
				socket_tos = IPTOS_MINCOST;
			} else if(!strcasecmp(cur2,"Normal-Service")) {
				socket_tos = 0;
			}
			xmlFree(cur2);
		}
		cur = cur->next;
	}


	if(!certf) {
		certf = ctrlproxy_path("cert.pem");
		if(access(certf, R_OK) != 0) { free(certf); certf = NULL; }
	}

	if(!keyf) {
		keyf = ctrlproxy_path("key.pem");
		if(access(keyf, R_OK) != 0) { free(keyf); keyf = NULL; }
	}

#if defined(HAVE_GNUTLS_GNUTLS_H)
	g_io_gnutls_init();

	if(!caf) {
		caf = ctrlproxy_path("ca.pem");
		if(access(caf, R_OK) != 0) { free(caf); caf = NULL; }
	}
		
	g_io_gnutls_set_files(certf, keyf, caf);
#elif defined(HAVE_OPENSSL_SSL_H)
	irssi_ssl_set_files(certf, keyf);
#endif

	if(certf) free(certf); 
	if(keyf) free(keyf); 
	if(caf) free(caf);

	return TRUE;
}
