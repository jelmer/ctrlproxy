/* 
	ctrlproxy: A modular IRC proxy
	(c) 2002 Jelmer Vernooij <jelmer@nl.linux.org>

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

#include "internals.h"

struct server *servers = NULL;

int login_server(struct server *s) {
	struct sockaddr_in servername;
	/* Create the socket. */
	s->socket = socket (PF_INET, SOCK_STREAM, 0);
	if (s->socket < 0)
	{
		perror ("socket (client)");
		s->socket = 0;
		return 1;
	}

	/* Connect to the server. */
	init_sockaddr (&servername, s->name, s->port);
	if (0 > connect (s->socket,
					 (struct sockaddr *) &servername,
					 sizeof (servername)))
	{
		perror ("connect (client)");
		s->socket = 0;
		return 1;
	}

	if(s->password)dprintf(s->socket, "PASS %s\n", s->password);
	dprintf(s->socket, "NICK %s\n", s->nick);
	dprintf(s->socket, "USER %s %s %s :%s\n", s->username, my_hostname, s->name, s->fullname);
	s->last_msg_time = time(NULL);
	s->ping_time = time(NULL);
	return 0;
}

struct server *connect_to_server(char *name, int port, char *nick, char *pass) {
	uid_t uid;
	struct passwd *pwd;
	struct server *s = malloc(sizeof(struct server));
	memset(s, 0, sizeof(struct server));
	s->name = strdup(name);
	s->nick = strdup(nick);
	s->password = pass?strdup(pass):NULL;
	s->port = port;

	uid = getuid();
	pwd = getpwuid(uid);
	s->username = strdup(pwd->pw_name);
	s->fullname = strdup(pwd->pw_gecos);
	asprintf(&s->hostmask, "%s!~%s@%s", s->nick, getenv("USER"), my_hostname);
	DLIST_ADD(servers, s);

	if(login_server(s))s->reconnect_time = time(NULL);

	return s;
}

int close_all(void)
{
	struct server *s = servers;
	while(s) {
		close_server(s);
		s = s->next;
	}
	return 0;
}

int close_server(struct server *s)
{
	struct module_context *c = s->handlers, *d;
	server_send_raw(s, "QUIT\r\n");
	while(c) {
		d = c->next;
		unload_module(c);
		c = d;
	}
	free(s->hostmask);
	DLIST_REMOVE(servers, s);
	return 0;
}

int loop_all(void)
{
	struct server *s = servers;
	int i = 0;

	while(s) {
		if(loop(s) != 0)i = -1;
		s = s->next;
	}
	return i;
}

int loop(struct server *server) /* Checks server socket for input and calls loop() on all of it's modules */
{
	fd_set activefd;
	struct line l;
	struct timeval tv;
	int retval;
	struct module_context *c;
	char *ret;
	static int timeout = -1, ping_interval = -1;
	char *conf;

	tv.tv_sec = 0;
	tv.tv_usec = 5;

	/* We need to reconnect */
	if(server->socket == 0) {
		conf = get_conf(server->abbrev, "reconnecttime");
		if(time(NULL) - server->reconnect_time < (conf?atoi(conf):30)) return 0;
		if(login_server(server) != 0)server->reconnect_time = time(NULL);
	}

	if(timeout == -1) {
		conf = get_conf(server->abbrev, "timeout");
		timeout = conf?atoi(conf):300;
	}

	if(ping_interval == -1) {
		conf = get_conf(server->abbrev, "ping_interval");
		ping_interval = conf?atoi(conf):100;
	}

	if(time(NULL) - server->last_msg_time > timeout) {
		server->socket = 0;
		server->reconnect_time = time(NULL);
		return 1;
	}
	
	if(time(NULL) - server->ping_time > ping_interval) {
		server_send(server, NULL, "PING", server->name, NULL);
		server->ping_time = time(NULL);
	}

	/* Call loop on all modules */
	c = server->handlers;

	while(c) {
		if(c->functions->loop)
			c->functions->loop(c);
		c = c->next;
	}

	/* Now, check for activity from the servers we're connected to */ 
	
	FD_ZERO(&activefd);
	FD_SET(server->socket, &activefd);
	retval = select(FD_SETSIZE, &activefd, NULL, NULL, &tv);

	if(!retval)return 0;

	if(aread(server->socket, &server->remaining) != 0) {
		server->socket = 0;
		server->reconnect_time = time(NULL);
		return 1;
	}

	while((ret = anextline(&server->remaining))) {
		server->last_msg_time = time(NULL);
		irc_parse_line(ret, &l);
		/* We only need to handle pings */
		if(!strcasecmp(l.args[0], "PING")){
			server_send(server, NULL, "PONG", l.args[1], NULL);
		}
		
		c = server->handlers;
		while(c) {
			if(c->functions->handle_incoming_data)
				c->functions->handle_incoming_data(c, &l);
			c = c->next;
		}
		free(ret);
	}
	return 0;
}

int server_send_raw(struct server *s, char *data)
{
	struct line l;
	struct module_context *c = s->handlers;
	irc_parse_line(data, &l);
	l.origin = s->hostmask;
	DEBUG("-> %s", data);

	while(c) {
		if(c->functions->handle_outgoing_data)
			c->functions->handle_outgoing_data(c, &l);
		c = c->next;
	}

	return write(s->socket, data, strlen(data));
}

int server_send(struct server *s, char *origin, ...) 
{
	va_list ap;
	char *arg;
	char nosplit = 0;
	char msg[IRC_MSG_LEN];
	strcpy(msg, "");
	va_start(ap, origin);

	if(origin) {
		snprintf(msg, IRC_MSG_LEN, ":%s ", origin);
	}

	while((arg = va_arg(ap, char *))) {
		if(nosplit)return -2;
		if(strchr(arg, ' ') || arg[0] == ':') { strcat(msg, ":"); nosplit = 1; }
		strcat(msg, arg);
		strcat(msg, " ");
	}
	strcat(msg, "\r\n");
	va_end(ap);

	return server_send_raw(s, msg);
}
