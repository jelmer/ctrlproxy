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
	ret = g_strdup_vprintf(fmt, ap);
	l = irc_parse_line(ret);
	g_free(ret);
	va_end(ap);
	return l;
}

struct line *virc_parse_line( const char *origin, va_list ap)
{
	char *arg;
	struct line *l;
	l = g_new0(struct line, 1);
	l->argc = 0;
	if(origin)l->origin = g_strdup(origin);
	else l->origin = NULL;
	
	l->args = g_new(char *, MAX_LINE_ARGS+2);
	
	while((arg = va_arg(ap, char *))) {
		if(l->argc > MAX_LINE_ARGS) {
			g_free(l); 
			g_free(l->args); 
			return NULL; 
		}
		l->args[l->argc] = g_strdup(arg);
		l->args = g_realloc(l->args, (((++l->argc)+2)* sizeof(char *)));
	}
	l->args[l->argc] = NULL;

	return l;
}

struct line * irc_parse_line(const char *d)
{
	char *p;
	char dosplit = 1;
	size_t estimate = 0;
	char *data = g_strdup(d);
	int i;
	struct line *l;

	l = g_new0(struct line, 1);
	l->has_endcolon = WITHOUT_COLON;
	p = data;

	if(p[0] == ':') {
		p = strchr(data, ' ');
		if(!p){ g_free(data); g_free(l); return NULL; }
		*p = '\0';
		l->origin = g_strdup(data+1);
		for(; *(p+1) == ' '; p++);
		p++;
	}

	for(i = 0; p[i]; i++) if(p[i] == ' ')estimate++;

	l->args = g_new(char *, estimate+2);

	l->args[0] = p;

	for(; *p; p++) {
		if(*p == ' ' && dosplit) {
			*p = '\0';
			l->argc++;
			l->args[l->argc] = p+1;
			if(*(p+1) == ':'){ 
				l->has_endcolon = WITH_COLON; 
				dosplit = 0; 
				l->args[l->argc]++; 
			}
		}

		if(*p == '\r' || *p == '\n') {
			*p = '\0';
			break;
		}
	}
	
	l->argc++;
	l->args[l->argc] = NULL;
	for(i = 0; l->args[i]; i++) l->args[i] = g_strdup(l->args[i]);
	g_free(data);

	return l;
}

gboolean irc_send_line(GIOChannel *c, struct line *l) {
	char *raw;
	GIOStatus ret;
	gsize bytes_written;

	if(!c) return FALSE;

	raw = irc_line_string_nl(l);
	ret = g_io_channel_write_chars(c, raw, -1, &bytes_written, NULL);

	switch (ret) {
	case G_IO_STATUS_AGAIN: g_message("AGAIN? "); break;
	case G_IO_STATUS_ERROR: g_message("ERORR!"); break;
	default: break;
	}
	
	g_free(raw);

	g_io_channel_flush(c, NULL);

	return (ret == G_IO_STATUS_NORMAL);
}


char *irc_line_string_nl(struct line *l) 
{
	char *raw = irc_line_string(l);
	raw = g_realloc(raw, strlen(raw)+10);
	strcat(raw, "\r\n");
	return raw;
}

int requires_colon(struct line *l)
{
	int c;

	if(l->has_endcolon == WITH_COLON) return 1;
	else if(l->has_endcolon == WITHOUT_COLON) return 0;

	c = atoi(l->args[0]);
	if(!g_strcasecmp(l->args[0], "MODE"))return 0;
	if(!g_strcasecmp(l->args[0], "NICK"))return 0;

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

	/* Silently ignore empty messages */
	if(l->argc == 0) return g_strdup("");

	if(l->origin)len+=strlen(l->origin);
	for(i = 0; l->args[i]; i++) len+=strlen(l->args[i])+2;
	ret = g_malloc(len+20);
	strcpy(ret, "");
	
	if(l->origin) sprintf(ret, ":%s ", l->origin);

	for(i = 0; i < l->argc; i++) {
		if(i == l->argc-1 && requires_colon(l) && i != 0)
			strcat(ret, ":");
		strcat(ret, l->args[i]);
		if(i != l->argc-1)strcat(ret, " ");
	}

	return ret;
}

void free_line(struct line *l) {
	int i;
	if(l->origin)g_free((char *)l->origin);
	if(l->args) {
		for(i = 0; l->args[i]; i++)g_free(l->args[i]);
		g_free(l->args);
	}
	l->args = NULL;
	l->origin = NULL;
	g_free(l);
}

char *line_get_nick(struct line *l)
{
	static char *nick = NULL;
	char *t;
	if(!l || !l->origin)return NULL;
	if(nick)g_free(nick);
	nick = g_strdup(l->origin);
	t = strchr(nick, '!');
	if(!t)return nick;
	*t = '\0';
	return nick;
}

gboolean irc_sendf(GIOChannel *c, char *fmt, ...) 
{
	va_list ap;
	char *r = NULL;
	struct line *l;
	gboolean ret;

	if(!c) return FALSE;

	va_start(ap, fmt);
	r = g_strdup_vprintf(fmt, ap);
	l = irc_parse_line(r);
	g_free(r);
	ret = irc_send_line(c, l);

	free_line(l); 

	return ret;
}

gboolean irc_send_args(GIOChannel *c, ...) 
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
	struct line *ret = g_memdup(l, sizeof(struct line));
	if(l->origin)ret->origin = g_strdup(l->origin);
	ret->has_endcolon = l->has_endcolon;
	ret->options = l->options;
	ret->args = g_new(char *, ret->argc+MAX_LINE_ARGS);
	for(i = 0; l->args[i]; i++) {
		ret->args[i] = g_strdup(l->args[i]);
	}
	ret->args[i] = NULL;
	return ret;
}


struct line *irc_recv_line(GIOChannel *c)
{
	gchar *raw;
	GIOStatus status;
	struct line *l;
	
	status = g_io_channel_read_line(c, &raw, NULL, NULL, NULL);

	if (status != G_IO_STATUS_NORMAL) {
		return NULL;
	}

	l = irc_parse_line(raw);

	g_free(raw);

	return l;
}

