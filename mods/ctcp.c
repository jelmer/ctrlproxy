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
#include <time.h>
#include "ctrlproxy.h"
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>

CTRLPROXY_VERSIONING_MAGIC

void mhandle_incoming_data(struct module_context *c, const struct line *l)
{
	char *data, *t, *msg, *dest, *dhostmask = NULL;
	time_t ti;
	if(strcasecmp(l->args[0], "PRIVMSG"))return;

	if(l->args[2][0] != '\001')return;
	data = strdup(l->args[2]+1);
	t = strchr(data, '\001');
	if(!t){ free(data); return; }
	*t = '\0';

	dest = l->args[1];
	if(!strcasecmp(dest, c->parent->nick)) {
		dhostmask = strdup(l->origin);
		t = strchr(dhostmask, '!');
		if(t)*t = '\0';
		dest = dhostmask;
	}

	t = strchr(data, ' ');
	if(t){ *t = '\0';t++; }

	if(!strcasecmp(data, "VERSION")) {
		struct utsname u;
		uname(&u);
		asprintf(&msg, "\001VERSION ctrlproxy:%s:%s %s\001", CTRLPROXY_VERSION, u.sysname, u.release);
		server_send(c->parent, c->parent->hostmask, "NOTICE", dest, msg, NULL);
		free(msg);
	} else if(!strcasecmp(data, "TIME")) {
		ti = time(NULL);
		asprintf(&msg, "\001TIME :%s\001", ctime(&ti));
		t = strchr(msg, '\n');
		if(t)*t = '\0';
		server_send(c->parent, c->parent->hostmask, "NOTICE", dest, msg, NULL);
		free(msg);
	} else if(!strcasecmp(data, "FINGER")) {
		asprintf(&msg, "\001FINGER %s\001", c->parent->fullname);
		server_send(c->parent, c->parent->hostmask, "NOTICE", dest, msg, NULL);
		free(msg);
	} else if(!strcasecmp(data, "SOURCE")) {
		server_send(c->parent, c->parent->hostmask, "NOTICE", dest, "\001SOURCE http://nl.linux.org/~jelmer/ctrlproxy/\001", NULL);
	} else if(!strcasecmp(data, "CLIENTINFO")) {
		server_send(c->parent, c->parent->hostmask, "NOTICE", dest, "\001CLIENTINFO ctrlproxy supports VERSION, TIME, FINGER, SOURCE, CLIENTINFO\001", NULL);
	} else if(!strcasecmp(data, "PING")) {
		server_send(c->parent, c->parent->hostmask, "NOTICE", dest, l->args[2], NULL);
	}

	free(data);
	if(dhostmask)free(dhostmask);
}

struct module_functions ctrlproxy_functions = {
	NULL, NULL, mhandle_incoming_data, NULL, NULL
};
