#ifndef __INTERNALS_H__
#define __INTERNALS_H__

#define _GNU_SOURCE
#include "dlinklist.h"
#include "ctrlproxy.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <arpa/inet.h>
#include <signal.h>
#include <dlfcn.h>

/* server.c: communication with server, parsing of lines */
struct server *connect_to_server(char *name, int port, char *nick, char *pass, char *fullname);
int loop(struct server *server); /* Checks server socket for input and calls loop() on all of it's modules */

/* modules.c: */
struct module_context *load_module(struct server *s, char *name);
int unload_module(struct module_context *); /* Removes specified module from server and unloads module if necessary */

#endif /* __INTERNALS_H__ */
