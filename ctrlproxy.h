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

#ifndef __CTRLPROXY_H__
#define __CTRLPROXY_H__

#define IRC_MSG_LEN 540
#define MAXHOSTNAMELEN 4096
#define CTRLPROXY_VERSION "0.5"

#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>

extern char output_debug;

#define DEBUG if(output_debug)printf

struct server;
struct line;

extern char my_hostname[MAXHOSTNAMELEN+2];

int server_send_raw(struct server *s, char *data);
int server_send(struct server *s, char *origin, ...);

/* conf.c */
char **enum_sections(void);
char *get_conf(char *section, char *name);

/* util.c */
void init_sockaddr (struct sockaddr_in *name, const char *hostname, uint16_t port);
int make_socket (uint16_t port);
int aread(int fd, char **remaining);
char *anextline(char **remaining);
int irc_parse_line(char *data, struct line *l);

struct line {
	char raw[IRC_MSG_LEN];
	char data[IRC_MSG_LEN];
	char *origin;
	char *args[IRC_MSG_LEN]; /* NULL terminated */
};

struct server {
	struct server *prev, *next;
	char *name;
	char *abbrev;
	char *username;
	char *fullname;
	char *hostmask;
	char *password;
	char *nick;
	time_t reconnect_time;
	time_t ping_time;
	time_t last_msg_time;
	int port;
	int socket;
	struct module_context *handlers;
	char *remaining;
};

struct module_context {
	struct module_context *prev, *next;
	struct server *parent;
	struct module_functions *functions;
	void *private_data;
	void *handle;
};

struct module_functions {
	void (*init)(struct module_context *);
	void (*loop)(struct module_context *);
	void (*handle_incoming_data)(struct module_context *, const struct line *);
	void (*handle_outgoing_data)(struct module_context *, const struct line *);
	void (*fini)(struct module_context *);
	struct module_functions *prev, *next;
};

#define CTRLPROXY_INTERFACE_VERSION 0
#define CTRLPROXY_VERSIONING_MAGIC int ctrlproxy_module_version = CTRLPROXY_INTERFACE_VERSION;

#endif /* __CTRLPROXY_H__ */
