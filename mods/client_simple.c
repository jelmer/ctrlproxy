#include "ctrlproxy.h"
#define _GNU_SOURCE
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <malloc.h>

CTRLPROXY_VERSIONING_MAGIC

struct client_state {
	int listen_socket;
	fd_set readfd;
	char *remaining[FD_SETSIZE];
};

void minit(struct module_context *c)
{
	struct client_state *st;

	if(!get_conf(c->parent->abbrev, "clientport"))return;
	
	st = malloc(sizeof(struct client_state));
	c->private_data = st;
	memset(st, 0, sizeof(struct client_state));
	FD_ZERO(&st->readfd);
	st->listen_socket = make_socket(atoi(get_conf(c->parent->abbrev, "clientport")));

	if(listen(st->listen_socket, 5) < 0)
	{
		fprintf(stderr, "Error trying to listen\n");
		free(st);
		return;
	}
	FD_SET(st->listen_socket, &st->readfd);
}

void mloop(struct module_context *c)
{
	int retval;
	struct client_state *st = (struct client_state *)c->private_data;
	fd_set activefd;
	struct timeval tv;
	int i;
	int sock;
	size_t size;
	struct sockaddr clientname;
	struct line l;
	char *ret;
	char *tmp;
	char allmodes[] = "aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ";
	tv.tv_usec = 5;
	tv.tv_sec = 0;
	if(!st)return;

	activefd = st->readfd;
	
	retval = select(FD_SETSIZE, &activefd, NULL, NULL, &tv);

	if(!retval)return;

	for(i = 0; i < FD_SETSIZE; i++) {
		if(!FD_ISSET(i, &activefd))continue;

		if(i == st->listen_socket) {
			size = sizeof(clientname);
			sock = accept(st->listen_socket, (struct sockaddr *)&clientname, &size);
			if(!sock)continue;
			FD_SET(sock, &st->readfd);
		} else {
			if(aread(i, &st->remaining[i]) != 0) {
				FD_CLR(i, &st->readfd);
				continue;
			}

			while((ret = anextline(&st->remaining[i]))) {
				irc_parse_line(ret, &l);
				if(!l.args[0])continue;
				if(strcasecmp(l.args[0], "USER")){
					asprintf(&tmp, "%s\n", ret);
					server_send_raw(c->parent, tmp);
					free(tmp);
				}
				else {
					dprintf(i, ":%s 001 %s :Welcome to the ctrlproxy\r\n", my_hostname, c->parent->nick);
					dprintf(i, ":%s 002 %s :Host %s is running ctrlproxy\r\n", my_hostname, c->parent->nick, my_hostname);
					dprintf(i, ":%s 003 %s :Ctrlproxy (c) 2002 Jelmer Vernooij <jelmer@nl.linux.org>\r\n", my_hostname, c->parent->nick);
					dprintf(i, ":%s 004 %s %s %s %s %s\r\n", my_hostname, c->parent->nick, CTRLPROXY_VERSION, allmodes, allmodes);
					dprintf(i, ":%s 422 %s :No MOTD file\r\n", my_hostname, c->parent->nick);
				}
				free(ret);
			}
		}
	}
}

void mhandle_incoming_data(struct module_context *c, const struct line *l)
{
	struct client_state *st = (struct client_state *)c->private_data;
	int i;
	if(!st)return;
	printf("To client: %s\r\n", l->raw);
	for(i = 0; i < FD_SETSIZE; i++) {
		if(FD_ISSET(i, &st->readfd) && i != st->listen_socket)
			dprintf(i, "%s\r\n", l->raw);
	}
}

void mfinish(struct module_context *c)
{
	struct client_state *st = (struct client_state *)c->private_data;
	int i;
	if(!st)return;
	for(i = 0; i < FD_SETSIZE; i++) {
		if(FD_ISSET(i, &st->readfd) && i != st->listen_socket)
			dprintf(i, ":%s SQUIT\r\n", my_hostname);
	}
}

struct module_functions ctrlproxy_functions = {
	minit, mloop, mhandle_incoming_data, NULL, mfinish
};
