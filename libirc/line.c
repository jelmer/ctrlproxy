/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2003 Jelmer Vernooĳ <jelmer@jelmer.uk>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
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

struct irc_line *irc_parse_line_args(const char *origin, ...)
{
	va_list ap;
	struct irc_line *l;
	va_start(ap, origin);
	l = virc_parse_line(origin, ap);
	va_end(ap);
	return l;
}

struct irc_line *irc_parse_linef(const char *fmt, ...)
{
	va_list ap;
	char *ret;
	struct irc_line *l;
	va_start(ap, fmt);
	ret = g_strdup_vprintf(fmt, ap);
	l = irc_parse_line(ret);
	g_free(ret);
	va_end(ap);
	return l;
}

struct irc_line *virc_parse_response(const char *from, const char *to, int response, va_list ap)
{
	struct irc_line *l;
	g_assert(response > 0);

	l = virc_parse_line(from, ap);

	l->args = g_realloc(l->args, sizeof(char *) * (l->argc+4));
	memmove(&l->args[2], &l->args[0], l->argc * sizeof(char *));

	l->args[0] = g_strdup_printf("%03d", response);
	l->args[1] = g_strdup(to);

	l->argc+=2;
	l->args[l->argc] = NULL;

	return l;
}

struct irc_line *virc_parse_line( const char *hostmask, va_list ap)
{
	char *arg;
	struct irc_line *l;

	l = g_new0(struct irc_line, 1);
	g_assert(l);
	l->argc = 0;
	if (hostmask != NULL)
		l->origin = g_strdup(hostmask);

	l->args = g_new(char *, MAX_LINE_ARGS+2);

	while((arg = va_arg(ap, char *))) {
		l->args[l->argc] = g_strdup(arg);
		l->args = g_realloc(l->args, (((++l->argc)+2) * sizeof(char *)));
	}
	l->args[l->argc] = NULL;

	return l;
}

struct irc_line * irc_parse_line(const char *d)
{
	char *p;
	char dosplit = 1;
	size_t estimate = 0;
	char *data = g_strdup(d);
	int i;
	struct irc_line *l;

	l = g_new0(struct irc_line, 1);
	g_assert(l);
	l->has_endcolon = WITHOUT_COLON;
	p = data;

	if (p[0] == ':') {
		p = strchr(data, ' ');
		if (!p){ g_free(data); g_free(l); return NULL; }
		*p = '\0';
		l->origin = g_strdup(data+1);
		for(; *(p+1) == ' '; p++);
		p++;
	}

	for(i = 0; p[i]; i++) if (p[i] == ' ')estimate++;

	l->args = g_new(char *, estimate+2);

	l->args[0] = p;

	for (; *p; p++) {
		if (*p == ' ' && dosplit) {
			*p = '\0';
			l->argc++;
			l->args[l->argc] = p+1;
			if (*(p+1) == ':'){
				l->has_endcolon = WITH_COLON;
				dosplit = 0;
				l->args[l->argc]++;
			}
		}

		if (*p == '\r' || *p == '\n') {
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

/**
 * Send a line over an IO Channel
 *
 * @param c IO Channel
 * @param iconv iconv to use, -1 for none
 * @param l Line
 * @param error Error
 */
GIOStatus irc_send_line(GIOChannel *c, GIConv iconv,
						const struct irc_line *l, GError **error)
{
	char *raw, *cvrt = NULL;
	GIOStatus ret;
	gsize bytes_written = 0;

	g_assert(c);

	raw = irc_line_string_nl(l);
	if (iconv != (GIConv)-1) {
		cvrt = g_convert_with_iconv(raw, -1, iconv, NULL, NULL, error);
		if (cvrt == NULL)
			return G_IO_STATUS_ERROR;
		g_free(raw);
	} else {
		cvrt = raw;
	}
	ret = g_io_channel_write_chars(c, cvrt, -1, &bytes_written, error);
	g_free(cvrt);

	if (ret == G_IO_STATUS_AGAIN) {
		g_assert(bytes_written == 0);
	}

	return ret;
}


char *irc_line_string_nl(const struct irc_line *l)
{
	char *raw;

	g_assert(l);
	raw = irc_line_string(l);
	g_assert(raw);

	raw = g_realloc(raw, strlen(raw)+10);
	strcat(raw, "\r\n");
	return raw;
}

static gboolean requires_colon(const struct irc_line *l)
{
	int c;

	g_assert(l);

	if (l->has_endcolon == WITH_COLON)
		return TRUE;
	else if (l->has_endcolon == WITHOUT_COLON)
		return FALSE;

	g_assert(l->args[0]);

	c = atoi(l->args[0]);
	if (!base_strcmp(l->args[0], "MODE"))
		return FALSE;

	if (!base_strcmp(l->args[0], "NICK"))
		return FALSE;

	if (!base_strcmp(l->args[0], "JOIN"))
		return FALSE;

	if (!base_strcmp(l->args[0], "PART") || !base_strcmp(l->args[0], "TOPIC")) {
		return (l->argc > 2);
    }

	switch(c) {
	case RPL_CHANNELMODEIS:
	case RPL_INVITING:
	case RPL_BANLIST:
	case RPL_TRACELINK:
	case RPL_TRACECONNECTING:
	case RPL_TRACEUNKNOWN:
	case RPL_TRACEOPERATOR:
	case RPL_CREATIONTIME:
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
	case RPL_MYINFO:
	case RPL_TOPICWHOTIME:
		return FALSE;

	default: return TRUE;
	}
}

char *irc_line_string(const struct irc_line *l)
{
	size_t len = 0; unsigned int i;
	char *ret;

	g_assert(l);

	/* Silently ignore empty messages */
	if (l->argc == 0) return g_strdup("");

	if (l->origin != NULL)
		len+=strlen(l->origin);
	for(i = 0; l->args[i]; i++) len+=strlen(l->args[i])+2;
	ret = g_malloc(len+20);
	strcpy(ret, "");

	if (l->origin != NULL)
		sprintf(ret, ":%s ", l->origin);

	for(i = 0; i < l->argc; i++) {
		if (i == l->argc-1 && requires_colon(l) && i != 0)
			strcat(ret, ":");
		strcat(ret, l->args[i]);
		if (i != l->argc-1)
			strcat(ret, " ");
	}

	return ret;
}

void free_line(struct irc_line *l)
{
	int i;
	if (l == NULL)
		return;

	if (l->origin != NULL)
		g_free(l->origin);

	if (l->args != NULL) {
		for(i = 0; l->args[i]; i++)g_free(l->args[i]);
		g_free(l->args);
	}
	l->args = NULL;
	l->origin = NULL;
	g_free(l);
}

char *line_get_nick(const struct irc_line *l)
{
	char *nick = NULL;
	char *t;
	g_assert(l != NULL);
	g_assert(l->origin != NULL);

	nick = g_strdup(l->origin);
	t = strchr(nick, '!');
	if (t == NULL)
		return nick;

	*t = '\0';
	return nick;
}

GIOStatus irc_sendf(GIOChannel *c, GIConv iconv,
					GError **error, char *fmt, ...)
{
	va_list ap;
	char *r = NULL;
	struct irc_line *l;
	GIOStatus ret;

	g_assert(c);
	g_assert(fmt);

	va_start(ap, fmt);
	r = g_strdup_vprintf(fmt, ap);
	l = irc_parse_line(r);
	g_free(r);
	ret = irc_send_line(c, iconv, l, error);

	free_line(l);

	return ret;
}

GIOStatus irc_send_args(GIOChannel *c, GIConv iconv,
						GError **error, ...)
{
	struct irc_line *l;
	GIOStatus ret;
	va_list ap;

	g_assert(c);

	va_start(ap, error);
	l = virc_parse_line(NULL, ap);
	va_end(ap);

	ret = irc_send_line(c, iconv, l, error);

	free_line(l);

	return ret;
}

struct irc_line *linedup(const struct irc_line *l)
{
	int i;
	struct irc_line *ret;
	g_assert(l);

	ret = g_memdup(l, sizeof(struct irc_line));

	if (l->origin != NULL)
		ret->origin = g_strdup(l->origin);
	ret->args = g_new(char *, ret->argc+MAX_LINE_ARGS);
	for(i = 0; l->args[i]; i++) {
		ret->args[i] = g_strdup(l->args[i]);
	}
	ret->args[i] = NULL;
	return ret;
}

GIOStatus irc_recv_line(GIOChannel *c, GIConv iconv,
						GError **error, struct irc_line **l)
{
	gchar *raw = NULL, *cvrt = NULL;
	GIOStatus status;
	gsize in_len;

	g_assert(l != NULL);

	*l = NULL;

	g_assert(c);

	status = g_io_channel_read_line(c, &raw, &in_len, NULL, error);
	if (status != G_IO_STATUS_NORMAL) {
		g_free(raw);
		return status;
	}

	if (iconv == (GIConv)-1) {
		cvrt = raw;
	} else {
		cvrt = g_convert_with_iconv(raw, -1, iconv, NULL, NULL, error);
		if (cvrt == NULL) {
			cvrt = raw;
			status = G_IO_STATUS_ERROR;
		} else {
			g_free(raw);
		}
	}

	*l = irc_parse_line(cvrt);

	g_free(cvrt);

	return status;
}

/* Estimate the length of a line */
static int line_len(const struct irc_line *l)
{
	int len = 0;
	int i;

	len = 4; /* \r\n for end of line, possibly colon before last argument and
				colon at start of the line */

	for (i = 0; i < l->argc; i++)
		len += strlen(l->args[i]) + 1;

	return len;
}

gboolean line_add_arg(struct irc_line *l, const char *arg)
{
	if (line_len(l) + strlen(arg) + 1 > IRC_MAXLINELEN)
		return FALSE;

	/* Check to see if this argument fits on the current line */
	l->args[l->argc] = g_strdup(arg);

	if (l->args[l->argc] == NULL)
		return FALSE;

	l->args = g_realloc(l->args, (((++l->argc)+2) * sizeof(char *)));

	if (l->args == NULL)
		return FALSE;

	l->args[l->argc] = NULL;

	return TRUE;
}

gboolean line_prefix_time(struct irc_line *l, time_t t)
{
	char stime[512];
	char *tmp;
	gboolean action = FALSE;

	if (l->args[2][0] == '\001') { /* CTCP */
		/* Let's not touch any CTCP requests but actions */
		if (strncmp(l->args[2]+1, "ACTION", strlen("ACTION")) != 0) {
			return FALSE;
		}
		action = TRUE;
	}

	strftime(stime, sizeof(stime), "%H:%M:%S", localtime(&t));
	if (action) {
		tmp = g_strdup_printf("\001ACTION [%s]%s", stime, l->args[2]+strlen(" ACTION"));
	} else {
		tmp = g_strdup_printf("[%s] %s", stime, l->args[2]);
	}
	if (tmp == NULL) {
		return FALSE;
	}
	g_free(l->args[2]);
	l->args[2] = tmp;

	return TRUE;
}

int irc_line_cmp(const struct irc_line *a, const struct irc_line *b)
{
	int i;
	int ret;

	if (a->origin != NULL && b->origin == NULL)
		return 1;

	if (a->origin == NULL && b->origin != NULL)
		return -1;

	if (a->origin == NULL && b->origin == NULL)
		ret = 0;
	else
		ret = strcmp(a->origin, b->origin);
	if (ret != 0)
		return ret;

	for (i = 0; i < MIN(a->argc, b->argc); i++) {
		ret = strcmp(a->args[i], b->args[i]);
		if (ret != 0)
			return ret;
	}

	return a->argc - b->argc;
}
