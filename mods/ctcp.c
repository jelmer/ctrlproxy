/* 
	ctrlproxy: A modular IRC proxy
	(c) 2002-2003 Jelmer Vernooij <jelmer@nl.linux.org>

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
#include "../config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "ctcp"

static gboolean mhandle_data(struct line *l)
{
	char *data, *t, *msg, *dest, *dhostmask = NULL;
	time_t ti;
	if(l->direction == TO_SERVER)return TRUE;
	if(strcasecmp(l->args[0], "PRIVMSG") || l->args[2][0] != '\001')return TRUE;
	data = strdup(l->args[2]+1);
	t = strchr(data, '\001');
	if(!t){ free(data); return TRUE; }
	*t = '\0';

	if(!l->origin)return TRUE;
	
	dhostmask = strdup(l->origin);
	t = strchr(dhostmask, '!');
	if(t)*t = '\0';
	dest = dhostmask;

	t = strchr(data, ' ');
	if(t){ *t = '\0';t++; }

	if(!strcasecmp(data, "VERSION")) {
		struct utsname u;
		uname(&u);
		asprintf(&msg, "\001VERSION ctrlproxy:%s:%s %s\001", PACKAGE_VERSION, u.sysname, u.release);
		irc_sendf(l->network->outgoing, "NOTICE %s :%s", dest, msg);
		free(msg);
	} else if(!strcasecmp(data, "TIME")) {
		ti = time(NULL);
		asprintf(&msg, "\001TIME %s\001", ctime(&ti));
		t = strchr(msg, '\n');
		if(t)*t = '\0';
		irc_sendf(l->network->outgoing, "NOTICE %s :%s", dest, msg);
		free(msg);
	} else if(!strcasecmp(data, "FINGER")) {
		char *fullname = xmlGetProp(l->network->xmlConf, "fullname");
		asprintf(&msg, "\001FINGER %s\001", fullname);
		xmlFree(fullname);
		irc_sendf(l->network->outgoing, "NOTICE %s :%s", dest, msg);
		free(msg);
	} else if(!strcasecmp(data, "SOURCE")) {
		irc_sendf(l->network->outgoing, "NOTICE %s :\001SOURCE http://nl.linux.org/~jelmer/ctrlproxy/\001", dest);
	} else if(!strcasecmp(data, "CLIENTINFO")) {
		irc_sendf(l->network->outgoing, "NOTICE %s :\001ACTION CLIENTINFO VERSION TIME FINGER SOURCE CLIENTINFO PING\001", dest);
	} else if(!strcasecmp(data, "PING")) {
		irc_sendf(l->network->outgoing, "NOTICE %s :%s", dest, l->args[2]?l->args[2]:"");
	} else if(!strcasecmp(data, "ACTION")) {
	} else if(!strcasecmp(data, "DCC")) {
	} else g_warning("Received unknown CTCP request '%s'!", data);

	free(data);
	if(dhostmask)free(dhostmask);
	return TRUE;
}

gboolean fini_plugin(struct plugin *p)
{
	del_filter(mhandle_data);
	return TRUE;
}

gboolean init_plugin(struct plugin *p) 
{
	add_filter("ctcp", mhandle_data);
	return TRUE;
}
