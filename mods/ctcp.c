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
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <sys/utsname.h>
#endif
#include "gettext.h"
#define _(s) gettext(s)

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "ctcp"

static gboolean mhandle_data(struct line *l)
{
	char *data, *t, *msg, *dest, *dhostmask = NULL;
	time_t ti;

	/* Don't answer our own CTCP requests */
	if(l->direction == TO_SERVER && l->argc > 2 && l->args[2][0] == '\001') {
		l->options|=LINE_IS_PRIVATE;
		return TRUE;
	}

	if(l->direction == TO_SERVER) return TRUE;

	if(g_strcasecmp(l->args[0], "PRIVMSG") || l->args[2][0] != '\001')return TRUE;

	data = g_strdup(l->args[2]+1);
	t = strchr(data, '\001');
	if(!t){ g_free(data); return TRUE; }
	*t = '\0';

	if(!l->origin)return TRUE;
	
	dhostmask = g_strdup(l->origin);
	t = strchr(dhostmask, '!');
	if(t)*t = '\0';
	dest = dhostmask;

	t = strchr(data, ' ');
	if(t){ *t = '\0';t++; }

	if(!g_strcasecmp(data, "VERSION")) {
#ifndef _WIN32
		struct utsname u;
		uname(&u);
		asprintf(&msg, "\001VERSION ctrlproxy:%s:%s %s\001", ctrlproxy_version(), u.sysname, u.release);
#else
		asprintf(&msg, "\001VERSION ctrlproxy:%s:Windows %d.%d\001", ctrlproxy_version(), _winmajor, _winminor);
#endif
		irc_sendf(l->network->outgoing, "NOTICE %s :%s", dest, msg);
		g_free(msg);
	} else if(!g_strcasecmp(data, "TIME")) {
		ti = time(NULL);
		asprintf(&msg, "\001TIME %s\001", ctime(&ti));
		t = strchr(msg, '\n');
		if(t)*t = '\0';
		irc_sendf(l->network->outgoing, "NOTICE %s :%s", dest, msg);
		g_free(msg);
	} else if(!g_strcasecmp(data, "FINGER")) {
		char *fullname = xmlGetProp(l->network->xmlConf, "fullname");
		asprintf(&msg, "\001FINGER %s\001", fullname);
		xmlFree(fullname);
		irc_sendf(l->network->outgoing, "NOTICE %s :%s", dest, msg);
		g_free(msg);
	} else if(!g_strcasecmp(data, "SOURCE")) {
		irc_sendf(l->network->outgoing, "NOTICE %s :\001SOURCE http://nl.linux.org/~jelmer/ctrlproxy/\001", dest);
	} else if(!g_strcasecmp(data, "CLIENTINFO")) {
		irc_sendf(l->network->outgoing, "NOTICE %s :\001ACTION CLIENTINFO VERSION TIME FINGER SOURCE CLIENTINFO PING\001", dest);
	} else if(!g_strcasecmp(data, "PING")) {
		irc_sendf(l->network->outgoing, "NOTICE %s :%s", dest, l->args[2]?l->args[2]:"");
	} else if(!g_strcasecmp(data, "ACTION")) {
	} else if(!g_strcasecmp(data, "DCC")) {
	} else g_warning(_("Received unknown CTCP request '%s'!"), data);

	g_free(data);
	if(dhostmask)g_free(dhostmask);
	return TRUE;
}

gboolean fini_plugin(struct plugin *p)
{
	del_filter_ex("client", mhandle_data);
	return TRUE;
}

const char name_plugin[] = "ctcp";

gboolean init_plugin(struct plugin *p) 
{
	add_filter_ex("ctcp", mhandle_data, "client", 1000);
	return TRUE;
}
