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
#include "config.h"
#include "ctrlproxy.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/un.h>
#include <unistd.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "socket"

enum ssl_mode { SSL_MODE_NONE = 0, SSL_MODE_SERVER = 1, SSL_MODE_CLIENT = 2};

/* Prototypes from network-openssl.c */
GIOChannel *irssi_ssl_get_iochannel(GIOChannel *handle, gboolean server);
gboolean irssi_ssl_set_files(char *certf, char *keyf);

static gboolean handle_in (GIOChannel *c, GIOCondition o, gpointer data);
static gboolean handle_disc (GIOChannel *c, GIOCondition o, gpointer data);

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
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "socketpair: %s", strerror(errno));
		return -1;
	}

	*f_in = sock[0];

	fcntl(sock[0], F_SETFL, O_NONBLOCK);

	pid = fork();
	if (pid == -1) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "fork: %s", strerror(errno));
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
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "Failed to exec %s : %s", command[0], strerror(errno));
		return -1;
	}

	close(sock[1]);

	return pid;
}

static void socket_to_iochannel(int sock, struct transport_context *c, enum ssl_mode ssl_mode)
{
	GIOChannel *ioc;
	struct socket_data *s = malloc(sizeof(struct socket_data));
	GError *error = NULL;
	ioc = g_io_channel_unix_new(sock);

#ifdef HAVE_OPENSSL_SSL_H
	if(ssl_mode != SSL_MODE_NONE) {
		GIOChannel *newioc;
		newioc = irssi_ssl_get_iochannel(ioc, ssl_mode == SSL_MODE_SERVER);

		if(!newioc) g_warning("Can't convert socket to SSL");
		else ioc = newioc;
	}
#endif
	
	g_io_channel_set_encoding(ioc, NULL, &error);
	if(error)g_error_free(error);

	g_io_channel_set_close_on_unref(ioc, TRUE);
	s->disc_id = g_io_add_watch(ioc, G_IO_IN, handle_in, (gpointer)c);
	s->in_id = g_io_add_watch(ioc, G_IO_HUP, handle_disc, (gpointer)c);

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
		else g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "Unknown element %s", cur->name);
	
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
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "Can't accept connection!");
		return FALSE;
	}
	g_log(G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE, "New client %d", sock);

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
		g_warning("Channel is not readeable!");
		return -1;
	}

	status = g_io_channel_read_line(ioc, &ret, NULL, NULL, &error);

	switch(status){
	case G_IO_STATUS_ERROR:
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "%s", (error?error->message:"Unknown error"));
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
	struct in_addr bind_host_ip4;
	struct in6_addr bind_host_ip6;


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

	g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "Connecting to %s:%d", hostname, port);

	sock = socket (domain, SOCK_STREAM, 0);

	if (sock < 0)
	{
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "(While connecting to server %s with %s): socket: %s", tempname, strerror(errno), c->functions->name);
		free(tempname);
		return -1;
	}

	if(ipv6) {
		name6.sin6_family = AF_INET6;
		name6.sin6_port = htons (port);
	} else {
		name4.sin_family = AF_INET;
		name4.sin_port = htons (port);
	}

	bind_addr = xmlGetProp(c->configuration, "bind");
	if(bind_addr) {
	 bind_host = gethostbyname2(bind_addr, family);
	 if(bind_host && ipv6) {
		memcpy((char *)&bind_host_ip6, bind_host->h_addr, bind_host->h_length);
	 } else if(bind_host)
  	 	bind_host_ip4 = (*((struct in_addr*) bind_host->h_addr_list[0]));
	}
	xmlFree(bind_addr);

	hostinfo = gethostbyname2 (hostname, family);

	if (hostinfo == NULL)
	{
		g_warning("Unknown host %s.", hostname);
		xmlFree(hostname);
		free(tempname);
		return -1;
	}

	if(ipv6) {
		memcpy((char *)&name6.sin6_addr, hostinfo->h_addr, hostinfo->h_length);
		if(bind_host)
			bind(sock, (struct sockaddr *)&bind_host_ip6, sizeof(name6));
		ret = connect(sock, (struct sockaddr *)&name6, sizeof(name6));
	} else {
		name4.sin_addr = *(struct in_addr *) hostinfo->h_addr;
		if(bind_host)
			name4.sin_addr.s_addr = bind_host_ip4.s_addr;
		ret = connect(sock, (struct sockaddr *)&name4, sizeof(name4));
	}

	if (ret < 0)
	{
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "(While connecting to server %s): connect: %s", tempname, strerror(errno));
		free(tempname);
		return -1;
	}
	free(tempname);


	ssl = xmlGetProp(c->configuration, "ssl");
	socket_to_iochannel(sock, c, (ssl && atoi(ssl))?SSL_MODE_CLIENT:SSL_MODE_NONE);
	xmlFree(ssl);

	g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "Successfully connected to %s:%d", hostname, port);
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
		g_message("Trying to send line '%s' to socket that is not connected!", l);
		return -1;
	}

	if(!(g_io_channel_get_flags(s->channel) & G_IO_FLAG_IS_WRITEABLE)) {
		g_warning("Channel is not writeable!");
		return -1;
	}

	if(g_io_channel_write_chars(s->channel, l, -1, NULL, &error) == G_IO_STATUS_ERROR)
	{
		g_message("Can't send: %s", (error?error->message:"g_io_channel_write_chars failed"));
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
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "socket: %s", strerror(errno));
		return -1;
	}

	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

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
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "bind: %s", strerror(errno));
		return -1;
	}

	if(listen(sock, 5) < 0) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "Error trying to listen on port %d: %s", port, strerror(errno));
		return -1;
	}

	g_log(G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE, "Listening on port %d(socket %d)", port, sock);

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
		g_warning("Error trying to listen on %s: %s", f, strerror(errno));
		free(f);
		return -1;
	}

	g_message("Listening on socket at %s(%d)", f, sock);

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
	return (unregister_transport("ipv4") &&
			unregister_transport("ipv6") && 
			unregister_transport("pipe"));
}

gboolean init_plugin(struct plugin *p) {
	xmlNodePtr cur;
	char *certf = NULL, *keyf = NULL, *defaultssl = NULL;;
	register_transport(&ipv4);
	register_transport(&ipv6);
	register_transport(&pipe_transport);

	cur = p->xmlConf->xmlChildrenNode;
	while(cur) {
		if(xmlIsBlankNode(cur) || !strcmp(cur->name, "comment")) { cur = cur->next; continue; }

		if(!strcmp(cur->name, "sslkeyfile")) keyf = xmlNodeGetContent(cur);
		else if(!strcmp(cur->name, "sslcertfile")) certf = xmlNodeGetContent(cur);
		cur = cur->next;
	}

#ifdef HAVE_OPENSSL_SSL_H
	if(!certf || !keyf) {
		defaultssl = ctrlproxy_path("ctrlproxy.pem");
		if(access(defaultssl, R_OK) == 0) {
			if(!certf) certf = strdup(defaultssl);
			if(!keyf) keyf = strdup(defaultssl);
			irssi_ssl_set_files(certf, keyf);
		}
		free(defaultssl);

	} else {
		irssi_ssl_set_files(certf, keyf);
	}
#endif
	free(certf); free(keyf);

	return TRUE;
}
