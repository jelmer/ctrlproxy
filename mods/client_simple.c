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

#define _GNU_SOURCE
#include "ctrlproxy.h"
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
	char authenticated[FD_SETSIZE];
	char **channels;
	int no_channels;
};

void ensure_channel(struct client_state *st, char *name) 
{
	int i;
	assert(st);
	for(i = 0; i < st->no_channels; i++) {
		if(!strcasecmp(st->channels[i], name))return;
	}
	st->channels = realloc(st->channels, (st->no_channels+2) * sizeof(char *));
	st->channels[st->no_channels] = strdup(name);
	st->no_channels++;
	st->channels[st->no_channels] = NULL;
}

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
	int i,j;
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
				st->authenticated[i] = 0;
				if(st->remaining[i])free(st->remaining[i]);
				st->remaining[i] = NULL;
				continue;
			}

			while((ret = anextline(&st->remaining[i]))) {
				irc_parse_line(ret, &l);
				if(!l.args[0])continue;
				if(!get_conf(c->parent->abbrev, "clientpass"))st->authenticated[i] = 1;
				if(!strcasecmp(l.args[0], "USER")){
					if(!st->authenticated[i]){
						dprintf(i, ":%s 451 %s :You are not registered\r\n", c->parent->name, c->parent->nick);
						continue;
					}
					dprintf(i, ":%s 001 %s :Welcome to the ctrlproxy\r\n", c->parent->name, c->parent->nick);
					dprintf(i, ":%s 002 %s :Host %s is running ctrlproxy\r\n", c->parent->name, c->parent->nick, my_hostname);
					dprintf(i, ":%s 003 %s :Ctrlproxy (c) 2002 Jelmer Vernooij <jelmer@nl.linux.org>\r\n", c->parent->name, c->parent->nick);
					dprintf(i, ":%s 004 %s %s %s %s %s\r\n", c->parent->name, c->parent->nick, c->parent->name, CTRLPROXY_VERSION, allmodes, allmodes);
					dprintf(i, ":%s 422 %s :No MOTD file\r\n", c->parent->name, c->parent->nick);

					/* Send JOIN for each channel */
					if(st->channels){
						for(j = 0; j < st->no_channels; j++) {
							/* Only for channels */
							if(st->channels[j][0] == '#' || st->channels[j][0] == '&') {
								dprintf(i, ":%s JOIN %s\r\n", c->parent->hostmask, st->channels[j]);
								server_send(c->parent, c->parent->hostmask, "NAMES", st->channels[j], NULL);
								server_send(c->parent, c->parent->hostmask, "TOPIC", st->channels[j], NULL);
							}
						}
					}
				} else if(!strcasecmp(l.args[0], "PASS")) {
					if (!get_conf(c->parent->abbrev, "clientpass"))
						st->authenticated[i] = 1;
					else if(!strcmp(l.args[1], get_conf(c->parent->abbrev, "clientpass"))) {
						st->authenticated[i] = 1;
					} else {
						dprintf(i, ":%s 464 %s :Password mismatch\r\n", c->parent->name, c->parent->nick);
					}
				} else if(!strcasecmp(l.args[0], "QUIT")) {
					/* Ignore */
				} else if(!strcasecmp(l.args[0], "PING")) {
					dprintf(i, ":%s PONG :%s\r\n", c->parent->name, l.args[1]);
				} else if(st->authenticated[i]) {
					asprintf(&tmp, "%s\n", ret);
					server_send_raw(c->parent, tmp);
					free(tmp);
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
	DEBUG("<- %s\n", l->raw);
	if(!strcasecmp(l->args[0], "JOIN")) ensure_channel(st,l->args[1]);
	for(i = 0; i < FD_SETSIZE; i++) {
		if(FD_ISSET(i, &st->readfd) && i != st->listen_socket && st->authenticated[i])
			dprintf(i, "%s\n", l->raw);
	}
}

void mfinish(struct module_context *c)
{
	struct client_state *st = (struct client_state *)c->private_data;
	int i;
	if(!st)return;
	for(i = 0; i < FD_SETSIZE; i++) {
		if(FD_ISSET(i, &st->readfd) && i != st->listen_socket)
			dprintf(i, ":%s QUIT :Server exiting\r\n", c->parent->name);
		FD_CLR(i, &st->readfd);
	}
	close(st->listen_socket);
}

struct module_functions ctrlproxy_functions = {
	minit, mloop, mhandle_incoming_data, NULL, mfinish
};
