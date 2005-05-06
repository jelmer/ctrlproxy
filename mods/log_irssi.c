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
#ifdef _WIN32
#include <direct.h>
#define mkdir(s,t) _mkdir(s)
#endif



#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "log_irssi"

static char *logfile = NULL;
static GHashTable *files = NULL;

static FILE *find_add_channel_file(struct network *s, const char *name) {
	char *n = NULL;
	FILE *f;
	char *hash_name;
	char *lowercase;

	lowercase = g_ascii_strdown(name?name:"messages", -1);
	hash_name = g_strdup_printf("%s/%s", s->name, lowercase);
	g_free(lowercase);

	f = g_hash_table_lookup(files, hash_name);
	if(!f) {
		char *cn; 
		const char *server_name;

		server_name = s->name;

		if(strchr(server_name, '/'))server_name = strrchr(server_name, '/');

		n = g_strdup_printf("%s/%s", logfile, server_name);
		/* Check if directory needs to be created */
		if(!g_file_test(n, G_FILE_TEST_IS_DIR) && mkdir(n, 0700) == -1) {
			log_network("log_irssi", s, "Couldn't create directory %s for logging!", n);
			g_free(hash_name);
			g_free(n);
			return NULL;
		}
		g_free(n);
		
		/* Then open the correct filename */
		cn = g_ascii_strdown(name?name:"messages", -1);
		n = g_strdup_printf("%s/%s/%s", logfile, server_name, cn);
		g_free(cn);
		f = fopen(n, "a+");
		if(!f) {
			log_network("log_irssi", s, "Couldn't open file %s for logging!", n);
			g_free(n);
			return NULL;
		}
		g_free(n);
		g_hash_table_insert(files, hash_name, f);
	} else g_free(hash_name);
	g_assert(f);
	return f;
}

static FILE *find_channel_file(struct network *s, const char *name) {
	FILE *f;
	char *hash_name, *lowercase;
	lowercase = g_ascii_strdown(name?name:"messages", -1);
	hash_name = g_strdup_printf("%s/%s", s->name, lowercase);
	g_free(lowercase);

	f = g_hash_table_lookup(files, hash_name);
	g_free(hash_name);
	return f;
}

static gboolean log_data(struct line *l, void *userdata)
{
	const char *nick = NULL;
	const char *dest = NULL;
	char *user = NULL;
	time_t ti = time(NULL);
	struct tm *t = localtime(&ti);
	FILE *f = NULL;
	if(!l->args || !l->args[0] || l->options & LINE_NO_LOGGING)return TRUE;
	if(l->origin)nick = l->origin;
	if(nick)user = strchr(nick, '!');
	if(user){ *user = '\0';user++; }
	if(!nick && l->network->nick)nick = l->network->nick;

	g_assert(l->args[0]);

	if(l->direction == FROM_SERVER && !g_strcasecmp(l->args[0], "JOIN")) {
		f = find_add_channel_file(l->network, l->args[1]);
		if(f)fprintf(f, "%02d:%02d -!- %s [%s] has joined %s\n", t->tm_hour, t->tm_min, nick, user, l->args[1]);
	} else if(l->direction == FROM_SERVER && !g_strcasecmp(l->args[0], "PART")) {
		f = find_add_channel_file(l->network, l->args[1]);
		if(f)fprintf(f, "%02d:%02d -!- %s [%s] has left %s [%s]\n", t->tm_hour, t->tm_min, nick, user, l->args[1], l->args[2]?l->args[2]:"");
	} else if(!g_strcasecmp(l->args[0], "PRIVMSG")) {
		dest = l->args[1];
		if(!irccmp(l->network, dest, l->network->nick))dest = nick;
		if(l->args[2][0] == '\001') { 
			l->args[2][strlen(l->args[2])-1] = '\0';
			if(!g_ascii_strncasecmp(l->args[2], "\001ACTION ", 8)) { 
				f = find_add_channel_file(l->network, dest);
				if(f)fprintf(f, "%02d:%02d  * %s %s\n", t->tm_hour, t->tm_min, nick, l->args[2]+8);
			}
			l->args[2][strlen(l->args[2])] = '\001';
			/* Ignore all other ctcp messages */
		} else {
			f = find_add_channel_file(l->network, dest);
			if(f)fprintf(f, "%02d:%02d < %s> %s\n", t->tm_hour, t->tm_min, nick, l->args[2]);
		}
	} else if(!g_strcasecmp(l->args[0], "MODE") && l->args[1] && is_channelname(l->args[1], l->network) && l->direction == FROM_SERVER) {
		f = find_add_channel_file(l->network, l->args[1]);
		if(f)fprintf(f, "%02d:%02d -!- mode/%s [%s %s] by %s\n", t->tm_hour, t->tm_min, l->args[1], l->args[2], l->args[3], nick);
	} else if(!g_strcasecmp(l->args[0], "QUIT")) {
		/* Loop thru the channels this user is on */
		GList *gl = l->network->channels;
		while(gl) {
			struct channel *c = (struct channel *)gl->data;
			if(find_nick(c, nick)) {
				f = find_channel_file(l->network, c->name);
				if(f)fprintf(f, "%02d:%02d -!- %s [%s] has quit [%s]\n", t->tm_hour, t->tm_min, nick, user, l->args[1]?l->args[1]:"");
			}
			gl = gl->next;
		}
	} else if(!g_strcasecmp(l->args[0], "KICK") && l->args[1] && l->args[2] && l->direction == FROM_SERVER) {
		if(!strchr(l->args[1], ',')) {
			f = find_add_channel_file(l->network, l->args[1]);
			if(f)fprintf(f, "%02d:%02d -!- %s has been kicked by %s [%s]\n", t->tm_hour, t->tm_min, l->args[2], nick, l->args[3]?l->args[3]:"");
		} else { 
			char *channels = g_strdup(l->args[1]);
			char *nicks = g_strdup(l->args[1]);
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
			
			g_free(channels);
			g_free(nicks);
		}
	} else if(!g_strcasecmp(l->args[0], "TOPIC") && l->direction == FROM_SERVER && l->args[1]) {
		f = find_add_channel_file(l->network, l->args[1]);
		if(f) {
			if(l->args[2])fprintf(f, "%02d:%02d -!- %s has changed the topic to %s\n", t->tm_hour, t->tm_min, nick, l->args[2]);
			else fprintf(f, "%02d:%02d -!- %s has removed the topic\n", t->tm_hour, t->tm_min, nick);
		}
	} else if(!g_strcasecmp(l->args[0], "NICK") && l->direction == FROM_SERVER && l->args[1]) {
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

	return TRUE;
}

static gboolean fini_plugin(struct plugin *p)
{
	g_hash_table_destroy(files);
	g_free(logfile); logfile = NULL;
	del_log_filter("log_irssi");
	return TRUE;
}

static gboolean save_config(struct plugin *p, xmlNodePtr node)
{
	xmlNewTextChild(node, NULL, "logfile", logfile);
	return TRUE;
}

static gboolean load_config(struct plugin *p, xmlNodePtr node) 
{
	xmlNodePtr cur;

	for (cur = node->children; cur; cur = cur->next)
	{
		if (cur->type != XML_ELEMENT_NODE) continue;

		if(!strcmp(cur->name, "logfile")) logfile = xmlNodeGetContent(cur);
	}

	if(!logfile) {
		logfile = ctrlproxy_path("log_irssi");
	}
	
	/* Create logfile directory if it doesn't exist yet */
	mkdir(logfile, 0700);

	return TRUE;
}

static gboolean init_plugin(struct plugin *p)
{
	files = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, fclose);
	add_log_filter("log_irssi", log_data, NULL, 1000);
	return TRUE;
}

struct plugin_ops plugin = {
	.name = "log_irssi",
	.version = 0,
	.init = init_plugin,
	.fini = fini_plugin,
	.load_config = load_config,
	.save_config = save_config,
};
