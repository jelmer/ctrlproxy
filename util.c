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

#define READ_BLOCK_SIZE 1024

void
init_sockaddr (struct sockaddr_in *name,
			   const char *hostname,
			   uint16_t port)
{
	struct hostent *hostinfo;

	name->sin_family = AF_INET;
	name->sin_port = htons (port);
	hostinfo = gethostbyname (hostname);
	if (hostinfo == NULL)
	{
		fprintf (stderr, "Unknown host %s.\n", hostname);
		exit (EXIT_FAILURE);
	}
	name->sin_addr = *(struct in_addr *) hostinfo->h_addr;
}

int make_socket (uint16_t port)
{
	int sock;
	struct sockaddr_in name;

	/* Create the socket. */
	sock = socket (PF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	{
		perror ("socket");
		exit (EXIT_FAILURE);
	}

	/* Give the socket a name. */
	name.sin_family = AF_INET;
	name.sin_port = htons (port);
	name.sin_addr.s_addr = htonl (INADDR_ANY);
	if (bind (sock, (struct sockaddr *) &name, sizeof (name)) < 0)
	{
		perror ("bind");
		exit (EXIT_FAILURE);
	}

	return sock;
}

int aread(int fd, char **remaining)
{
	char *msg = *remaining;
	size_t len = 0;
	int retread;

	if(msg)len = strlen(msg);
	
	msg = realloc(msg, (len+READ_BLOCK_SIZE+20));
	retread = recv(fd, msg+len, READ_BLOCK_SIZE, 0);
	if(retread == -1) {
		perror("recv");
		return -1;
	}
	msg[len+retread] = '\0';
	*remaining = msg;
	return 0;
}

char *anextline(char **remaining)
{
	char *nl, *ret, *new;
	nl = strchr(*remaining, '\n');
	if(!nl)return NULL;
	*nl = '\0';
	new = strdup(nl+1);
	ret = strdup(*remaining);
	free(*remaining);
	*remaining = new;
	return ret;
}

int irc_parse_line(char *data, struct line *l)
{
	char *p;
	char dosplit = 1;
	int argc;

	if(strlen(data) >= IRC_MSG_LEN) return -1;

	memset(l, 0, sizeof(struct line));
	strcpy(l->raw, data);
	strcpy(l->data, data);
	argc = 0;
	p = l->data;

	if(p[0] == ':') {
		
		l->origin = p+1;
		p = strchr(l->data, ' ');
		if(!p)return -1;
		for(; *(p+1) == ' '; p++);
		*p = '\0';
		p++;
	}

	l->args[0] = p;

	for(; *p; p++) {
		if(*p == ' ' && dosplit) {
			*p = '\0';
			argc++;
			l->args[argc] = p+1;
			if(*(p+1) == ':'){ dosplit = 0; l->args[argc]++; }
		}
		if(*p == '\r') {
			*p = '\0';
			break;
		}
	}
	argc++;
	l->args[argc] = NULL;
	return 0;
}
