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
#include "ctrlproxy.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <glib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "gettext.h"
#define _(s) gettext(s)


#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "log_irssi"

static char *logfile = NULL;
static GHashTable *files = NULL;

static FILE *find_add_channel_file(struct network *s, char *name) {
	char *n = NULL;
	FILE *f;
	char *server_name, *hash_name;
	char *lowercase;
	server_name = xmlGetProp(s->xmlConf, "name");

	lowercase = g_ascii_strdown(name?name:"messages", -1);
	asprintf(&hash_name, "%s/%s", server_name, lowercase);
	free(lowercase);

	xmlFree(server_name);

	f = g_hash_table_lookup(files, hash_name);
	if(!f) {
		char *cn, *server_name;

		server_name = xmlGetProp(s->xmlConf, "name");

		if(strchr(server_name, '/'))server_name = strrchr(server_name, '/');

		asprintf(&n, "%s/%s", logfile, server_name);
		/* Check if directory needs to be created */
		if(access(n, F_OK) != 0 && mkdir(n, 0700) == -1) {
			g_warning(_("Couldn't create directory %s for logging!"), n);
			free(hash_name);
			free(n);
			xmlFree(server_name);
			return NULL;
		}
		free(n);
		
		/* Then open the correct filename */
		cn = g_ascii_strdown(name?name:"messages", -1);
		asprintf(&n, "%s/%s/%s", logfile, server_name, cn);
		xmlFree(server_name);
		g_free(cn);
		f = fopen(n, "a+");
		if(!f) {
			g_warning(_("Couldn't open file %s for logging!"), n);
			free(n);
			return NULL;
		}
		free(n);
		g_hash_table_insert(files, hash_name, f);
	} else free(hash_name);
	g_assert(f);
	return f;
}

static FILE *find_channel_file(struct network *s, char *name) {
	FILE *f;
	char *server_name, *hash_name, *lowercase;
	server_name = xmlGetProp(s->xmlConf, "name");

	lowercase = g_ascii_strdown(name?name:"messages", -1);
	asprintf(&hash_name, "%s/%s", server_name, lowercase);
	g_free(lowercase);

	xmlFree(server_name);

	f = g_hash_table_lookup(files, hash_name);
	free(hash_name);
	return f;
}

static gboolean log_data(struct line *l)
{
	char *nick = NULL;
	char *dest = NULL;
	char *user = NULL;
	time_t ti = time(NULL);
	struct tm *t = localtime(&ti);
	FILE *f = NULL;
	if(!l->args || !l->args[0] || l->options & LINE_NO_LOGGING)return TRUE;
	if(l->origin)nick = strdup(l->origin);
	if(nick)user = strchr(nick, '!');
	if(user){ *user = '\0';user++; }
	if(!nick && xmlHasProp(l->network->xmlConf, "nick"))nick = xmlGetProp(l->network->xmlConf, "nick");

	g_assert(l->args[0]);

	if(l->direction == FROM_SERVER && !strcasecmp(l->args[0], "JOIN")) {
		f = find_add_channel_file(l->network, l->args[1]);
		if(f)fprintf(f, "%02d:%02d -!- %s [%s] has joined %s\n", t->tm_hour, t->tm_min, nick, user, l->args[1]);
	} else if(l->direction == FROM_SERVER && !strcasecmp(l->args[0], "PART")) {
		f = find_add_channel_file(l->network, l->args[1]);
		if(f)fprintf(f, "%02d:%02d -!- %s [%s] has left %s [%s]\n", t->tm_hour, t->tm_min, nick, user, l->args[1], l->args[2]?l->args[2]:"");
	} else if(!strcasecmp(l->args[0], "PRIVMSG")) {
		char *nnick = xmlGetProp(l->network->xmlConf, "nick");
		dest = l->args[1];
		if(!irccmp(l->network, dest, nnick))dest = nick;
		xmlFree(nnick);
		if(l->args[2][0] == '') { 
			l->args[2][strlen(l->args[2])-1] = '\0';
			if(!strncasecmp(l->args[2], "ACTION ", 8)) { 
				f = find_add_channel_file(l->network, dest);
				if(f)fprintf(f, "%02d:%02d  * %s %s\n", t->tm_hour, t->tm_min, nick, l->args[2]+8);
			}
			l->args[2][strlen(l->args[2])] = '';
			/* Ignore all other ctcp messages */
		} else {
			f = find_add_channel_file(l->network, dest);
			if(f)fprintf(f, "%02d:%02d < %s> %s\n", t->tm_hour, t->tm_min, nick, l->args[2]);
		}
	} else if(!strcasecmp(l->args[0], "MODE") && l->args[1] && is_channelname(l->args[1], l->network) && l->direction == FROM_SERVER) {
		f = find_add_channel_file(l->network, l->args[1]);
		if(f)fprintf(f, "%02d:%02d -!- mode/%s [%s %s] by %s\n", t->tm_hour, t->tm_min, l->args[1], l->args[2], l->args[3], nick);
	} else if(!strcasecmp(l->args[0], "QUIT")) {
		/* Loop thru the channels this user is on */
		GList *gl = l->network->channels;
		while(gl) {
			struct channel *c = (struct channel *)gl->data;
			if(find_nick(c, nick)) {
				char *channame = xmlGetProp(c->xmlConf, "name");
				f = find_channel_file(l->network, channame);
				xmlFree(channame);
				if(f)fprintf(f, "%02d:%02d -!- %s [%s] has quit [%s]\n", t->tm_hour, t->tm_min, nick, user, l->args[1]?l->args[1]:"");
			}
			gl = gl->next;
		}
	} else if(!strcasecmp(l->args[0], "KICK") && l->args[1] && l->args[2] && l->direction == FROM_SERVER) {
		if(!strchr(l->args[1], ',')) {
			f = find_add_channel_file(l->network, l->args[1]);
			if(f)fprintf(f, "%02d:%02d -!- %s has been kicked by %s [%s]\n", t->tm_hour, t->tm_min, l->args[2], nick, l->args[3]?l->args[3]:"");
		} else { 
			char *channels = strdup(l->args[1]);
			char *nicks = strdup(l->args[1]);
			char *p,*n; char cont = 1;
			char *_nick;

			p = channels;
			_nick = nicks;
			while(cont) {
				n = strchr(p, ',');

				if(!n) cont = 0;
				else *n = '\0';

				f = find_add_channel_file(l->network, p);
				if(f)fprintf(f, "%02d:%02d -!- %s has been kicked by %s [%s]\n", t->tm_hour, t->tm_min, _nick, nick, l->args[3]?l->args[3]:"");

				p = n+1;
				_nick = strchr(_nick, ',');
				if(!_nick)break;
				_nick++;
			}
			
			free(channels);
			free(nicks);
		}
	} else if(!strcasecmp(l->args[0], "TOPIC") && l->direction == FROM_SERVER && l->args[1]) {
		f = find_add_channel_file(l->network, l->args[1]);
		if(f) {
			if(l->args[2])fprintf(f, "%02d:%02d -!- %s has changed the topic to %s\n", t->tm_hour, t->tm_min, nick, l->args[2]);
			else fprintf(f, "%02d:%02d -!- %s has removed the topic\n", t->tm_hour, t->tm_min, nick);
		}
	} else if(!strcasecmp(l->args[0], "NICK") && l->direction == FROM_SERVER && l->args[1]) {
		/* Loop thru the channels this user is on */
		GList *gl = l->network->channels;
		while(gl) {
			struct channel *c = (struct channel *)gl->data;
			if(find_nick(c, nick)) {
				f = find_channel_file(l->network, nick);
				if(f)fprintf(f, "%02d:%02d -!- %s is now known as %s\n", t->tm_hour, t->tm_min, nick, l->args[1]);
			}
			gl = gl->next;
		}
	}

	if(f)fflush(f);

	if(nick)free(nick);

	return TRUE;
}

gboolean fini_plugin(struct plugin *p)
{
	free(logfile); logfile = NULL;
	del_filter_ex("log", log_data);
	return TRUE;
}

const char name_plugin[] = "log_irssi";

gboolean init_plugin(struct plugin *p)
{
	xmlNodePtr cur = p->xmlConf->xmlChildrenNode;
	while(cur) {
		if(!xmlIsBlankNode(cur) && !strcmp(cur->name, "logfile")) logfile = xmlNodeGetContent(cur);
		cur = cur->next;
	}

	if(!logfile) {
		logfile = ctrlproxy_path("log_irssi");
	}
	
	/* Create logfile directory if it doesn't exist yet */
	mkdir(logfile, 0600);

	files = g_hash_table_new(g_str_hash, g_str_equal);
	add_filter_ex("log_irssi", log_data, "log", 1000);
	return TRUE;
}
