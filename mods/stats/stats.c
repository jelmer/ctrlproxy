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

/* Keep track of certain 'events' for every nick and for total */

#define _GNU_SOURCE
#include "ctrlproxy.h"
#include "config.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <time.h>
#include <tdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_REGEX_SUBMATCHES 30

#ifdef HAVE_PCRE_H
# include <pcre.h>
#endif

#ifdef HAVE_REGEX_H
# include <regex.h>
#endif

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "stats"

#define DO_INCREASE(n) increase_item(network, channel, nick, n, 1); 

static TDB_CONTEXT *tdb_context = NULL;
static GList *patterns = NULL;

struct pattern {
	char *string;
	char *type;
#ifdef HAVE_REGEX_H
	regex_t preg;
#endif
#ifdef HAVE_PCRE_H
	pcre *re;
#endif
};

#if !defined(HAVE_REGEX_H) && !defined(HAVE_PCRE_H)
static int match_pattern(char *str, struct pattern *p)
	if(strstr(str, p->string) != NULL)return 1;
	return 0;
}

static int compile_pattern(struct pattern *p) { return 0; }

#elif !defined(HAVE_PCRE_H)

static void g_log_regex_error(int ret, regex_t *preg)
{
	char errbuf[4096];
	regerror(ret, preg, errbuf, sizeof(errbuf));
	g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "Regex error: %s", errbuf);
}

static int match_pattern(char *str, struct pattern *p)
{
	regmatch_t matches[MAX_REGEX_SUBMATCHES];
	int i = 0, ret = 0, offset = 0;
	while(ret == 0) {
		ret = regexec(&p->preg, str+offset, MAX_REGEX_SUBMATCHES, matches, REG_NOTBOL | REG_NOTEOL);
		if(ret == REG_NOMATCH || matches[0].rm_eo == 0) return i;
		if(ret)g_log_regex_error(ret, &p->preg);	
		
		i++;
		offset+= matches[0].rm_eo;
	}
		
	return 0;
}

static int compile_pattern(struct pattern *p)
{
	int ret;
	ret = regcomp(&p->preg, p->string, REG_EXTENDED);
	if(ret) { g_log_regex_error(ret, &p->preg); return -1; }
	return 0;
}



#else
	
static int match_pattern(char *str, struct pattern *p)
{
	int ovector[MAX_REGEX_SUBMATCHES];
	int rc, i = 1;
	rc = pcre_exec( p->re, NULL, str, strlen(str), 0, 0, ovector, MAX_REGEX_SUBMATCHES);
	if(rc == PCRE_ERROR_NOMATCH)return 0;

	for(;;)
	{
		int options = 0;
		int start_offset = ovector[1];

		if(ovector[0] == ovector[1])
		{
			if(ovector[0] == strlen(str)) break;
			options = PCRE_NOTEMPTY | PCRE_ANCHORED;
		}

		rc = pcre_exec(p->re, NULL, str, strlen(str), start_offset, options, ovector, MAX_REGEX_SUBMATCHES);

		if (rc == PCRE_ERROR_NOMATCH) {
			if(options == 0) return i;
			ovector[1] = start_offset + 1;
			continue;
		} else i++;
	}
	return 0;
}

static int compile_pattern(struct pattern *p)
{
	const char *error;
	int erroffset;
    p->re = pcre_compile(p->string, 0, &error, &erroffset, NULL);
	if(p->re == NULL) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "PCRE compilation failed at offset %d: %s\n", erroffset, error);
		return -1;
	}
	return 0;
}

#endif

static void increase_item(const char *server, const char *channel, const char *nick, const char *value, long ch)
{
	TDB_DATA d, r;
	char *key;
	long *ivalue;
	if(ch == 0)return;
	asprintf(&key, "%s/%s/%s/%s", server, channel == NULL?"":channel, nick == NULL?"":nick, value);
	d.dptr = key;
	d.dsize = strlen(key)+1;
	r = tdb_fetch(tdb_context, d);
	if(r.dptr == NULL) { /* First time value is set */
		r.dptr = malloc(sizeof(long));
		r.dsize = sizeof(long);
		ivalue = (long *)r.dptr;
		*ivalue = 0;
	}
	ivalue = (long *)r.dptr;
	(*ivalue)+=ch;
	tdb_store(tdb_context, d, r, TDB_REPLACE);
	free(r.dptr);
	free(key);
}

int has_caps(char *a) 
{
	int i; int capsseen = 0;
	for(i = 0; a[i]; i++) {
		if(a[i] >= 'A' && a[i] <= 'Z') capsseen++;
		if(a[i] >= 'a' && a[i] <= 'z') 
			return 1;
	}

	return capsseen > 2;
}

static gboolean log_data(struct line *l)
{
	char *nick = NULL;
	GList *g;
	char *network = NULL;
	const char *channel;
	if(!l->args || !l->args[0] || !l->args[1])return TRUE;
	
	/* Only log channels */
	if(!is_channelname(l->args[1], l->network))return TRUE;

	channel = l->args[1];

	if(line_get_nick(l))nick = strdup(line_get_nick(l)); 
	else nick = xmlGetProp(l->network->xmlConf, "nick");
	
	network = xmlGetProp(l->network->xmlConf, "name");

	if(!strcasecmp(l->args[0], "JOIN"))DO_INCREASE("joins");
	if(!strcasecmp(l->args[0], "PART") || !strcasecmp(l->args[0], "QUIT"))DO_INCREASE("parts");
	if(!strcasecmp(l->args[0], "PRIVMSG")) {
		time_t tmp = time(NULL);
		struct tm *t = localtime(&tmp);
		char buf[10];
		/* Time */
		snprintf(buf, 10, "hour-%d", t->tm_hour);
		DO_INCREASE(buf);
		DO_INCREASE("lines");

		if(has_caps(l->args[2])) 
			DO_INCREASE("caps");
		
		g = patterns;
		while(g) {
			struct pattern *p = (struct pattern *)g->data;
			increase_item(network, channel, nick, p->type, match_pattern(l->args[2], p));
			g = g->next;
		}
	}

	if(!strcasecmp(l->args[0], "TOPIC")) {
		increase_item(network, channel, nick, "topicchange", 1);
	}
	
	if(!strcasecmp(l->args[0], "KICK")){ 
		increase_item(network, channel, nick, "dokick", 1);
		increase_item(network, channel, l->args[1], "getkick", 1);
	}

	if(!strcasecmp(l->args[0], "MODE") && !strncasecmp(l->args[1], "+o", 2)) {
		increase_item(network, channel, nick, "doop", 1);
		increase_item(network, channel, l->args[1], "getop", 1);
	}

	if(!strcasecmp(l->args[0], "MODE") && !strncasecmp(l->args[1], "-o", 2)) {
		increase_item(network, channel, nick, "takeop", 1);
		increase_item(network, channel, l->args[1], "optaken", 1);
	}
	free(nick);
	xmlFree(network);

	return TRUE;
}

gboolean fini_plugin(struct plugin *p)
{
	del_filter_ex("log", log_data);
	return TRUE;
}

const char name_plugin[] = "stats";

gboolean init_plugin(struct plugin *p)
{
	xmlNodePtr cur = p->xmlConf->xmlChildrenNode;
	char *statsdb = NULL;
	while(cur) {
		if(!xmlIsBlankNode(cur) && !strcmp(cur->name, "tdbfile"))statsdb = xmlNodeGetContent(cur);
		else if(!xmlIsBlankNode(cur) && !strcmp(cur->name, "pattern")) {
			struct pattern *p = malloc(sizeof(struct pattern));
			p->string = xmlNodeGetContent(cur);
			p->type = xmlGetProp(cur, "type");

			if(compile_pattern(p) == 0) patterns = g_list_append(patterns, p);

		}
		cur = cur->next;
	}

	if(!statsdb) statsdb = ctrlproxy_path("stats");	

	tdb_context = tdb_open(statsdb, 0, 0, O_RDWR | O_CREAT, 00700);

	if(!tdb_context) {
		g_warning("Unable to open TDB database %s\n", statsdb);
		free(statsdb);
		return FALSE;
	}

	free(statsdb);

	add_filter_ex("stats", log_data, "log", 1000);
	return TRUE;
}
