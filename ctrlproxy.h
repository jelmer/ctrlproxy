#ifndef __CTRLPROXY_H__
#define __CTRLPROXY_H__

#define IRC_MSG_LEN 540
#define MAXHOSTNAMELEN 4096
#define CTRLPROXY_VERSION "0.1"

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

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
	char *hostmask;
	char *nick;
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
