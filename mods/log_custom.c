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

#define MAX_SUBST 256
#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "log_custom"

static xmlNodePtr xmlConf = NULL;

/* Translation table */
struct log_mapping {
	char *command;
	char subst;
	int index;
	/* If index is -1 */
	char *(*callback) (struct line *l, gboolean case_sensitive);
};

static char *get_hours(struct line *l, gboolean case_sensitive) { 
	char *ret;
	time_t ti = time(NULL);
	struct tm *t = localtime(&ti);
	asprintf(&ret, "%02d", t->tm_hour);
	return ret;
}

static char *get_minutes(struct line *l, gboolean case_sensitive) { 
	char *ret;
	time_t ti = time(NULL);
	struct tm *t = localtime(&ti);
	asprintf(&ret, "%02d", t->tm_min);
	return ret;
}

static char *get_seconds(struct line *l, gboolean case_sensitive) { 
	char *ret;
	time_t ti = time(NULL);
	struct tm *t = localtime(&ti);
	asprintf(&ret, "%02d", t->tm_sec);
	return ret;
}

static char *get_nick(struct line *l, gboolean case_sensitive) {
	if(line_get_nick(l)) {
		if(case_sensitive) return g_ascii_strdown(line_get_nick(l), -1);
		else return strdup(line_get_nick(l)); 
	}
	
	if(l->direction == TO_SERVER) return xmlGetProp(l->network->xmlConf, "nick");
	
	return strdup("");
}

static char *get_network(struct line *l, gboolean case_sensitive) 
{ return xmlGetProp(l->network->xmlConf, "name"); }
static char *get_server(struct line *l, gboolean case_sensitive)
{ return xmlGetProp(l->network->current_server, "name"); }

static char *get_percent(struct line *l, gboolean case_sensitive) { return strdup("%"); }

static char *identifier = NULL;

static char *get_identifier(struct line *l, gboolean case_sensitive) { 
	if(case_sensitive) return g_ascii_strdown(identifier, -1); 
	else return strdup(identifier); 
}

static struct log_mapping mappings[] = {
	{NULL, '@', -1, get_identifier },
	{NULL, 'h', -1, get_hours },
	{NULL, 'M', -1, get_minutes },
	{NULL, 's', -1, get_seconds },
	{NULL, 'n', -1, get_nick },
	{NULL, 'N', -1, get_network },
	{NULL, 'S', -1, get_server },
	{NULL, '%', -1, get_percent },
	{"JOIN", 'c', 1, NULL },
	{"PART", 'c', 1, NULL },
	{"PART", 'm', 2, NULL },
	{"KICK", 'c', 1, NULL },
	{"KICK", 't', 2, NULL },
	{"KICK", 'r', 3, NULL },
	{"QUIT", 'm', 1, NULL },
	{"NOTICE", 't', 1, NULL },
	{"NOTICE", 'm', 2, NULL },
	{"PRIVMSG", 't', 1, NULL },
	{"PRIVMSG", 'm', 2, NULL },
	{"TOPIC", 'c', 1, NULL },
	{"TOPIC", 't', 2, NULL },
	{"MODE", 't', 1, NULL },
	{"MODE", 'p', 2, NULL },
	{"NICK", 'r', 1, NULL },
	{NULL, '0', 0, NULL },
	{NULL, '1', 1, NULL },
	{NULL, '2', 2, NULL },
	{NULL, '3', 3, NULL },
	{NULL, '4', 4, NULL },
	{NULL, '5', 5, NULL },
	{NULL, '6', 6, NULL },
	{NULL, '7', 7, NULL },
	{NULL, '8', 8, NULL },
	{NULL, '9', 9, NULL },
	{ NULL }
};

static char *find_mapping(struct line *l, char c, gboolean case_sensitive)
{
	int i;
	for(i = 0; mappings[i].subst; i++) {
		if(mappings[i].command && 
		   strcmp(mappings[i].command, l->args[0])) continue;
		if(mappings[i].subst != c)continue;
		if(mappings[i].index == -1) return mappings[i].callback(l, case_sensitive);
		if(mappings[i].index < l->argc) {
			if(case_sensitive) return g_ascii_strdown(l->args[mappings[i].index], -1);
			else return strdup(l->args[mappings[i].index]);
		}
	}
	return strdup("");
}

static void custom_subst(char **_new, char *fmt, struct line *l, char *_identifier, gboolean case_sensitive)
{
	char *subst[MAX_SUBST];
	char *new;
	size_t len, curpos = 0;
	int i;

	identifier = _identifier;

	len = strlen(fmt);
	memset(subst, 0, sizeof(char *) * MAX_SUBST);
	for(i = 0; i < strlen(fmt); i++) {
		if(fmt[i] == '%') {
			subst[(int)fmt[i+1]] = find_mapping(l, fmt[i+1], case_sensitive);	
			len += strlen(subst[(int)fmt[i+1]]);
		}
	}

	len++; /* newline! */

	new = malloc(len);
	for(i = 0; i < strlen(fmt); i++) {
		if(fmt[i] == '%') {
			new[curpos] = '\0';
			strncat(new, subst[(int)fmt[i+1]], len);
			curpos+=strlen(subst[(int)fmt[i+1]]);
			i++;
		} else {
			new[curpos] = fmt[i];
			curpos++;
		}
	}
	new[curpos] = '\0';

	for(i = 0; i < MAX_SUBST; i++) { if(subst[i])free(subst[i]); }
	*_new = new;
}

/* Syntax:
Always:
 * %h -> hours
 * %m -> minutes
 * %s -> seconds
 * %n -> initiating nick
 * %u -> initiating user
 * %N -> network name
 * %S -> server name
 * %% -> percent sign
If appropriate:
 * %c -> channel name
 * %m -> message

 -- JOIN: %c
 -- PART: %c, %m
 -- KICK: %t (target nick), %r (reason)
 -- QUIT: %m
 -- NOTICE/PRIVMSG: %t (target nick/channel), %m
 -- MODE: %p(mode change), %c, %t (target nick)
 -- TOPIC: %t
 -- NICK: %r
 */

static GHashTable *files = NULL;

static FILE *find_channel_file(struct line *l, char *identifier) {
	char *n = NULL;
	xmlNodePtr cur = xmlFindChildByElementName(xmlConf, "logfilename");
	FILE *f;
	char *logfilename;
	if(!cur) return NULL;
	logfilename = xmlNodeGetContent(cur);
	if(!logfilename)return NULL;
	custom_subst(&n, logfilename, l, identifier, TRUE);
	free(logfilename);
	f = g_hash_table_lookup(files, n);
	free(n);
	return f;
}

static FILE *find_add_channel_file(struct line *l, char *identifier) {
	char *n = NULL, *dn, *p;
	FILE *f;
	xmlNodePtr cur = xmlFindChildByElementName(xmlConf, "logfilename");
	char *logfilename;
	if(!cur) return NULL;

	logfilename = xmlNodeGetContent(cur);
	if(!logfilename) return NULL;
	custom_subst(&n, logfilename, l, identifier, TRUE);
	f = g_hash_table_lookup(files, n);
	if(!f) {
		dn = strdup(n);
		
		/* Only include directory-part */
		p = strrchr(dn, '/');
		if(p) *p = '\0';

		/* Check if directory needs to be created */
		if(access(dn, F_OK) != 0 && mkdir(dn, 0700) == -1) {
			g_warning("Couldn't create directory %s for logging!", dn);
			xmlFree(logfilename);
			free(dn);
			free(n);
			return NULL;
		}
		free(dn);
		
		/* Then open the correct filename */
		custom_subst(&n, logfilename, l, identifier, TRUE);
		f = fopen(n, "a+");
		if(!f) {
			g_warning("Couldn't open file %s for logging!", n);
			xmlFree(logfilename);
			free(n);
			return NULL;
		}
		g_hash_table_insert(files, n, f);
		free(n);
	} else free(n);
	xmlFree(logfilename);
	return f;
}

static void file_write_target(const char *n, struct line *l) 
{
	char *t, *s, *fmt;
	xmlNodePtr cur;
	FILE *f;
	char *own_nick = xmlGetProp(l->network->xmlConf, "nick");

	cur = xmlFindChildByElementName(xmlConf, n);
	if(!cur) return;

	fmt = xmlNodeGetContent(cur);
	if(!fmt) return;


	if(!irccmp(l->network, own_nick, l->args[1])) {
		if(line_get_nick(l)) { t = strdup(line_get_nick(l)); }
		else { t = strdup("_messages_"); }
	} else {
		t = strdup(l->args[1]);
	}
	xmlFree(own_nick);

	f = find_add_channel_file(l, t);
	if(!f) { free(t); return; }
	
	custom_subst(&s, fmt, l, t, FALSE);
	free(t);
	xmlFree(fmt);

    fputs(s, f);
	fputc('\n', f);
	fflush(f);

	free(s);
}

static void file_write_channel_only(const char *n, struct line *l)
{
	char *s, *fmt;
	xmlNodePtr cur;
	FILE *f;

	cur = xmlFindChildByElementName(xmlConf, n);
	if(!cur) return;

	fmt = xmlNodeGetContent(cur);
	if(!fmt)return;

	f = find_add_channel_file(l, l->args[1]);
	if(!f) return; 

	custom_subst(&s, fmt, l, l->args[1], FALSE);
	xmlFree(fmt);

	fputs(s, f); fputc('\n', f);
	fflush(f);

	free(s);
}

static void file_write_channel_query(const char *n, struct line *l)
{
	char *s, *fmt;
	xmlNodePtr cur;
	char *nick;
	FILE *f;
	GList *gl;

	nick = line_get_nick(l);
	if(!nick)return;

	cur = xmlFindChildByElementName(xmlConf, n);
	if(!cur)return;
	fmt = xmlNodeGetContent(cur);
	if(!fmt)return;

	/* check for the query first */
	f = find_channel_file(l, nick);

	if(f) {
		custom_subst(&s, fmt, l, nick, FALSE);
		fputs(s, f); fputc('\n', f);
		fflush(f);
		free(s);
	}
	
	/* now, loop thru the channels and check if the user is there */
	gl = l->network->channels;
	while(gl) {
		struct channel *c = (struct channel *)gl->data;
		if(find_nick(c, nick)) {
			char *channame = xmlGetProp(c->xmlConf, "name");
			f = find_add_channel_file(l, channame);
			if(f) {
				custom_subst(&s, fmt, l, channame, FALSE);
				fputs(s, f); fputc('\n', f);
				fflush(f);
				free(s);
			}
			xmlFree(channame);
		}
		gl = gl->next;
	}
	
	xmlFree(fmt);
}

static gboolean log_custom_data(struct line *l)
{
	char *nick = NULL;
	char *user = NULL;
	FILE *f = NULL;
	if(!l->args || !l->args[0] || l->options & LINE_NO_LOGGING)return TRUE;
	if(l->origin)nick = strdup(l->origin);
	if(nick)user = strchr(nick, '!');
	if(user){ *user = '\0';user++; }
	if(!nick && xmlHasProp(l->network->xmlConf, "nick"))nick = xmlGetProp(l->network->xmlConf, "nick");

	printf("Writing logs for line of %s\n", l->args[0]);
	
	/* Loop thru possible values for %@ */

	/* There are a few possibilities:
	 * - log to line_get_nick(l) or l->args[1] file depending on which 
	 *   was the current user (PRIVMSG, NOTICE, ACTION, MODE) (target)
	 * - log to all channels line_get_nick(l) was on, and to query, if applicable (NICK, QUIT) (channel_query)
	 * - log to channel only (KICK, PART, JOIN, TOPIC) (channel_only)
	 */

	if(l->direction == FROM_SERVER && !strcasecmp(l->args[0], "JOIN")) {
		file_write_target("join", l); 
	} else if(l->direction == FROM_SERVER && !strcasecmp(l->args[0], "PART")) {
		file_write_channel_only("part", l);
	} else if(!strcasecmp(l->args[0], "PRIVMSG")) {
		if(l->args[2][0] == '') { 
			l->args[2][strlen(l->args[2])-1] = '\0';
			if(!strncasecmp(l->args[2], "ACTION ", 8)) { 
				l->args[2]+=8;
				file_write_target("action", l);
				l->args[2]-=8;
			}
			l->args[2][strlen(l->args[2])] = '';
			/* Ignore all other ctcp messages */
		} else {
			file_write_target("msg", l);
		}
	} else if(!strcasecmp(l->args[0], "NOTICE")) {
		file_write_target("notice", l);
	} else if(!strcasecmp(l->args[0], "MODE") && l->args[1] && is_channelname(l->args[1], l->network) && l->direction == FROM_SERVER) {
		file_write_target("mode", l);
	} else if(!strcasecmp(l->args[0], "QUIT")) {
		file_write_channel_query("quit", l);
	} else if(!strcasecmp(l->args[0], "KICK") && l->args[1] && l->args[2] && l->direction == FROM_SERVER) {
		if(!strchr(l->args[1], ',')) {
			file_write_channel_only("kick", l);
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

				file_write_channel_only("kick", l);

				p = n+1;
				_nick = strchr(_nick, ',');
				if(!_nick)break;
				_nick++;
			}
			
			free(channels);
			free(nicks);
		}
	} else if(!strcasecmp(l->args[0], "TOPIC") && l->direction == FROM_SERVER && l->args[1]) {
		if(l->args[2]) file_write_channel_only("topic", l);
		else file_write_channel_only("notopic", l);
	} else if(!strcasecmp(l->args[0], "NICK") && l->direction == FROM_SERVER && l->args[1]) {
		file_write_channel_query("nickchange", l);
	}

	if(f) fflush(f);

	if(nick)free(nick);

	return TRUE;
}

gboolean fini_plugin(struct plugin *p)
{
	del_filter(log_custom_data);
	return TRUE;
}

gboolean init_plugin(struct plugin *p)
{
	xmlConf = p->xmlConf;
	g_assert(p->xmlConf);
	if(!xmlFindChildByElementName(xmlConf, "logfilename")) {
		g_warning("No <logfilename> tag for log_custom module");
		return FALSE;
	}
	files = g_hash_table_new(g_str_hash, g_str_equal);
	add_filter("log_custom", log_custom_data);
	return TRUE;
}
