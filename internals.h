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

#ifndef __INTERNALS_H__
#define __INTERNALS_H__

#define _GNU_SOURCE
#include <popt.h>
#include "dlinklist.h"
#include "ctrlproxy.h"
#include <pwd.h>
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
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>

/* conf.c */
int load_conf_file(char *file);

/* server.c: communication with server, parsing of lines */
struct server *connect_to_server(char *name, int port, char *nick, char *pass);
int loop_all(void);
int loop(struct server *server); /* Checks server socket for input and calls loop() on all of it's modules */
int close_all(void);
int close_server(struct server *s);

/* modules.c: */
struct module_context *load_module(struct server *s, char *name);
int unload_module(struct module_context *); /* Removes specified module from server and unloads module if necessary */

#endif /* __INTERNALS_H__ */
