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
#include "dlinklist.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <time.h>

CTRLPROXY_VERSIONING_MAGIC

struct channel {
	struct channel *next, *prev;
	FILE *log;
	char *name;
};


FILE *find_channel(struct channel *channels, struct server *s, char *name)
{
	char *location;
	struct channel *c = channels;
	time_t t;
	if(!get_conf(s->abbrev, "logfile"))return NULL;
	while(c) {
		if(!strcmp(c->name, name))return c->log;
		c = c->next;
	}
	c = malloc(sizeof(struct channel));
	memset(c, 0, sizeof(struct channel));
	asprintf(&location, get_conf(s->abbrev, "logfile"), s->abbrev, name);
	c->name = strdup(name);
	c->log = fopen(location, "a+");
	DLIST_ADD(channels, c);
	t = time(NULL);
	fprintf(c->log, "--- log opened %s\n", ctime(&t));
	return c->log;
}

void channel_log(struct module_context *c, char *channel, char *fmt, ... )
{
	va_list ap;
	FILE *f;
    struct channel *channels = (struct channel *)c->private_data;
	f = find_channel(channels, c->parent, channel);
	if(!f)return;
	va_start(ap, fmt);
	vfprintf(f, fmt, ap);
	va_end(ap);
}

void mhandle_data(struct module_context *c, const struct line *l)
{
	char *nick = NULL;
	char *dest = NULL;
	char *user = NULL;
	time_t ti = time(NULL);
	struct tm *t = localtime(&ti);
	if(l->origin)nick = strdup(l->origin);
	if(nick)user = strchr(nick, '!');
	if(user){
		*user = '\0';user++;
	}
	
	if(!strcasecmp(l->args[0], "JOIN")) {
		channel_log(c, l->args[1], "%2d:%2d -!- %s [%s] has joined %s\n", t->tm_hour, t->tm_min, nick, user, l->args[1]);
	} else if(!strcasecmp(l->args[0], "PART")) {
		channel_log(c, l->args[1], "%2d:%2d -!- %s [%s] has left %s [%s]\n", t->tm_hour, t->tm_min, nick, user, l->args[1], l->args[2]?l->args[2]:"");
	} else if(!strcasecmp(l->args[0], "PRIVMSG")) {
		dest = l->args[1];
		if(!strcasecmp(dest, c->parent->nick))dest = nick;
		if(!strncasecmp(l->args[2], "ACTION ", 8))
			channel_log(c, dest, "%2d:%2d  * %s %s\n", t->tm_hour, t->tm_min, nick, l->args[2]+8);
		else 
			channel_log(c, dest, "%2d:%2d < %s> %s\n", t->tm_hour, t->tm_min, nick, l->args[2]);
	} else if(!strcasecmp(l->args[0], "MODE") && (l->args[1][1] == '#' || l->args[1][1] == '&')) {
		channel_log(c, l->args[1], "%2d:%2d -!- mode/%s [%s %s] by %s\n", t->tm_hour, t->tm_min, l->args[1], l->args[2], l->args[3], nick);
	}
}

void mfinish(struct module_context *m)
{
    struct channel *channels = (struct channel *)m->private_data;
	struct channel *c = channels;
	while(c) {
		fclose(c->log);
		c = c->next;
	}
}

struct module_functions ctrlproxy_functions = {
	NULL, NULL, mhandle_data, mhandle_data, mfinish
};
