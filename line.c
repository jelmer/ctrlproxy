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

#define MAX_LINE_ARGS 64

#include "internals.h"
#include "irc.h"

extern FILE *debugfd;

struct line *irc_parse_line_args( char *origin, ... ) {
	va_list ap;
	struct line *l;
	va_start(ap, origin);
	l = virc_parse_line(origin, ap);
	va_end(ap);
	return l;
}

struct line *irc_parse_linef( char *fmt, ... ) {
	va_list ap;
	char *ret;
	struct line *l;
	va_start(ap, fmt);
	vasprintf(&ret, fmt, ap);
	l = irc_parse_line(ret);
	free(ret);
	va_end(ap);
	return l;
}

struct line *virc_parse_line( char *origin, va_list ap)
{
	char *arg;
	struct line *l;
	l = calloc(1, sizeof(struct line));
	l->argc = 0;
	if(origin)l->origin = strdup(origin);
	else l->origin = NULL;
	
	l->args = malloc(sizeof(char *) * (MAX_LINE_ARGS+2));
	
	while((arg = va_arg(ap, char *))) {
		if(l->argc > MAX_LINE_ARGS) {
			free(l); 
			free(l->args); 
			return NULL; 
		}
		l->args[l->argc] = strdup(arg);
		l->args = realloc(l->args, (((++l->argc)+2)* sizeof(char *)));
	}
	l->args[l->argc] = NULL;

	return l;
}

struct line * irc_parse_line(char *d)
{
	char *p;
	char dosplit = 1;
	size_t estimate = 0;
	char *data = strdup(d);
	int i;
	struct line *l;

	l = calloc(1, sizeof(struct line));
	p = data;

	if(p[0] == ':') {
		p = strchr(data, ' ');
		if(!p){ free(l); return NULL; }
		*p = '\0';
		l->origin = strdup(data+1);
		for(; *(p+1) == ' '; p++);
		p++;
	}

	for(i = 0; p[i]; i++) if(p[i] == ' ')estimate++;

	l->args = malloc(sizeof(char *) * (estimate+2));

	l->args[0] = p;

	for(; *p; p++) {
		if(*p == ' ' && dosplit) {
			*p = '\0';
			l->argc++;
			l->args[l->argc] = p+1;
			if(*(p+1) == ':'){ dosplit = 0; l->args[l->argc]++; }
		}

		if(*p == '\r') {
			*p = '\0';
			break;
		}
	}
	
	l->argc++;
	l->args[l->argc] = NULL;
	for(i = 0; l->args[i]; i++) l->args[i] = strdup(l->args[i]);
	free(data);

	return l;
}

char *irc_line_string_nl(struct line *l) 
{
	char *raw = irc_line_string(l);
	raw = realloc(raw, strlen(raw)+10);
	strcat(raw, "\r\n");
	return raw;
}

int requires_colon(char *ch)
{
	int c = atoi(ch);
	if(!strcasecmp(ch, "MODE"))return 0;
	if(!strcasecmp(ch, "NICK"))return 0;

	switch(c) {
	case RPL_CHANNELMODEIS:
	case RPL_INVITING:
	case RPL_BANLIST:
	case RPL_TRACELINK:
	case RPL_TRACECONNECTING:
	case RPL_TRACEUNKNOWN:
	case RPL_TRACEOPERATOR:
	case RPL_TRACEUSER:
	case RPL_TRACESERVER:
	case RPL_TRACENEWTYPE:
	case RPL_TRACELOG:
	case RPL_STATSLINKINFO:
	case RPL_STATSCOMMANDS:
	case RPL_STATSCLINE:
	case RPL_STATSNLINE:
	case RPL_STATSILINE:
	case RPL_STATSKLINE:
	case RPL_STATSYLINE:
	case RPL_ENDOFSTATS:
	case RPL_STATSLLINE:
	case RPL_STATSUPTIME:
	case RPL_STATSOLINE:
	case RPL_STATSHLINE:
	case RPL_UMODEIS:
		return 0;

	default: return 1;
	}
}

char *irc_line_string(struct line *l) {
	size_t len = 0; unsigned int i;
	char *ret;
	g_assert(l);

	/* Silently ignore empty messages */
	if(l->argc == 0) return strdup("");

	if(l->origin)len+=strlen(l->origin);
	for(i = 0; l->args[i]; i++) len+=strlen(l->args[i])+2;
	ret = malloc(len+20);
	strcpy(ret, "");
	
	if(l->origin) sprintf(ret, ":%s ", l->origin);

	for(i = 0; i < l->argc; i++) {
		if(i == l->argc-1 && requires_colon(l->args[0]) && i != 0)
			strcat(ret, ":");
		strcat(ret, l->args[i]);
		if(i != l->argc-1)strcat(ret, " ");
	}

	return ret;
}

void free_line(struct line *l) {
	int i;
	if(l->origin)free(l->origin);
	if(l->args) {
		for(i = 0; l->args[i]; i++)free(l->args[i]);
		free(l->args);
	}
	l->args = NULL;
	l->origin = NULL;
	free(l);
}

char *line_get_nick(struct line *l)
{
	static char *nick = NULL;
	char *t;
	if(!l || !l->origin)return NULL;
	if(nick)free(nick);
	nick = strdup(l->origin);
	t = strchr(nick, '!');
	if(!t)return nick;
	*t = '\0';
	return nick;
}

gboolean irc_send_line(struct transport_context *c, struct line *l) {
	char *raw;
	int ret;

	if(!c) return FALSE;

	raw = irc_line_string_nl(l);
	ret = transport_write(c, raw);
	free(raw);

	return (ret != -1);
}

gboolean irc_sendf(struct transport_context *c, char *fmt, ...) 
{
	va_list ap;
	char *r = NULL;
	struct line *l;
	gboolean ret;

	if(!c) return FALSE;

	va_start(ap, fmt);
	vasprintf(&r, fmt, ap);
	l = irc_parse_line(r);
	free(r);
	ret = irc_send_line(c, l);

	free_line(l); 

	return ret;
}

gboolean irc_send_args(struct transport_context *c, ...) 
{
	struct line *l;
	gboolean ret;
	va_list ap;

	if(!c) return FALSE;
	
	va_start(ap, c);
	l = virc_parse_line(NULL, ap);
	va_end(ap);

	ret = irc_send_line(c, l);

	free_line(l); l = NULL;

	return ret;
}

struct line *linedup(struct line *l) {
	int i;
	struct line *ret = calloc(1, sizeof(struct line));
	memcpy(ret, l, sizeof(struct line));
	if(l->origin)ret->origin = strdup(l->origin);
	ret->options = l->options;
	ret->args = malloc(sizeof(char *) * (ret->argc+MAX_LINE_ARGS));
	for(i = 0; l->args[i]; i++) {
		ret->args[i] = strdup(l->args[i]);
	}
	ret->args[i] = NULL;
	return ret;
}
