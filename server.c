#include "internals.h"

struct server *servers = NULL;

struct server *connect_to_server(char *name, int port, char *nick, char *fullname, char *pass) {
	struct sockaddr_in servername;
	struct server *s = malloc(sizeof(struct server));
	memset(s, 0, sizeof(struct server));
	s->name = name;

	/* Create the socket. */
	s->socket = socket (PF_INET, SOCK_STREAM, 0);
	if (s->socket < 0)
	{
		perror ("socket (client)");
		return NULL;
	}

	/* Connect to the server. */
	init_sockaddr (&servername, name, port);
	if (0 > connect (s->socket,
					 (struct sockaddr *) &servername,
					 sizeof (servername)))
	{
		perror ("connect (client)");
		return NULL;
	}


	if(pass)dprintf(s->socket, "PASS %s\n", pass);
	dprintf(s->socket, "NICK %s\n", nick);
	dprintf(s->socket, "USER %s %s %s :%s\n", getenv("USER"), "FIXME", name, fullname);

	DLIST_ADD(servers, s);

	return s;
}

int loop(struct server *server) /* Checks server socket for input and calls loop() on all of it's modules */
{
	fd_set activefd;
	struct line l;
	struct timeval tv;
	int retval;
	struct module_context *c;
	char *ret;

	tv.tv_sec = 0;
	tv.tv_usec = 5;
	
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
		/* FIXME: Reconnect */
		return 1;
	}

	while((ret = anextline(&server->remaining))) {
		irc_parse_line(ret, &l);
		/* We only need to handle pings */
		if(!strcasecmp(l.args[0], "PING")){
			server_send(server, NULL, "PONG", l.args[1], NULL);
		}
		printf("%s\n", l.args[0]);
		c = server->handlers;
		while(c) {
			if(c->functions->handle_data)
				c->functions->handle_data(c, &l);
			c = c->next;
		}
		free(ret);
	}
	return 0;
}

int server_send_raw(struct server *s, char *data)
{
	write(s->socket, data, strlen(data));
}

int server_send(struct server *s, char *origin, ...) 
{
	va_list ap;
	char *arg;
	char nosplit = 0;
	char msg[IRC_MSG_LEN];
	strcpy(msg, "");

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

	return server_send_raw(s, msg);
}
