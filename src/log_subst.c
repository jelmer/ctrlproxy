/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2007 Jelmer Vernooij <jelmer@jelmer.uk>

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

#include "internals.h"

struct subst_context {
	const char *identifier;
	struct irc_network *network;
};

/**
 * Translation table
 */
struct log_mapping {
	char *command;
	char subst;
	size_t index;
	/* If index is -1 */
	char *(*callback) (struct subst_context *subst_ctx,
					   const struct irc_line *line,
					   gboolean case_sensitive);
};

static char *get_hours(struct subst_context *subst_ctx, const struct irc_line *line, gboolean case_sensitive)
{
	time_t ti = time(NULL);
	struct tm *t = localtime(&ti);
	return g_strdup_printf("%02d", t->tm_hour);
}

static char *get_minutes(struct subst_context *subst_ctx, const struct irc_line *line, gboolean case_sensitive)
{
	time_t ti = time(NULL);
	struct tm *t = localtime(&ti);
	return g_strdup_printf("%02d", t->tm_min);
}

static char *get_seconds(struct subst_context *subst_ctx, const struct irc_line *line, gboolean case_sensitive)
{
	time_t ti = time(NULL);
	struct tm *t = localtime(&ti);
	return g_strdup_printf("%02d", t->tm_sec);
}

static char *get_seconds_since_1970(struct subst_context *subst_ctx, const struct irc_line *line, gboolean case_sensitive)
{
	time_t ti = time(NULL);
	return g_strdup_printf("%ld", ti);
}

static char *get_day(struct subst_context *subst_ctx, const struct irc_line *line, gboolean case_sensitive)
{
	time_t ti = time(NULL);
	struct tm *t = localtime(&ti);
	return g_strdup_printf("%02d", t->tm_mday);
}

static char *get_month(struct subst_context *subst_ctx, const struct irc_line *line, gboolean case_sensitive)
{
	time_t ti = time(NULL);
	struct tm *t = localtime(&ti);
	return g_strdup_printf("%02d", t->tm_mon + 1);
}

static char *get_year(struct subst_context *subst_ctx, const struct irc_line *line, gboolean case_sensitive)
{
	time_t ti = time(NULL);
	struct tm *t = localtime(&ti);
	return g_strdup_printf("%04d", t->tm_year + 1900);
}

static char *get_user(struct subst_context *subst_ctx, const struct irc_line *line, gboolean case_sensitive)
{
	char *user = NULL;

	if (line->origin != NULL) {
		user = strchr(line->origin, '!');
		if (user != NULL)
			user++;
	}

	if (case_sensitive)
		return g_ascii_strdown(user, -1);
	else
		return g_strdup(user);
}

static char *get_monthname(struct subst_context *subst_ctx, const struct irc_line *line, gboolean case_sensitive)
{
	char stime[512];
	time_t ti = time(NULL);
	strftime(stime, sizeof(stime), "%b", localtime(&ti));
	return g_strdup_printf("%s", stime);
}

static char *get_nick(struct subst_context *subst_ctx, const struct irc_line *line, gboolean case_sensitive)
{
	if (line->origin != NULL) {
		char *n = line_get_nick(line);
		if (case_sensitive) {
			char *r = g_ascii_strdown(n, -1);
			g_free(n);
			return r;
		}
		else return n;
	}
	
	return g_strdup("");
}

static char *get_network(struct subst_context *subst_ctx, const struct irc_line *line, gboolean case_sensitive)
{
	return g_strdup(subst_ctx->network->name);
}

static char *get_server(struct subst_context *subst_ctx, const struct irc_line *line, gboolean case_sensitive)
{
	if (subst_ctx->network->connection.data.tcp.current_server)
		return g_strdup(subst_ctx->network->connection.data.tcp.current_server->host);
	return g_strdup("");
}

static char *get_percent(struct subst_context *subst_ctx, const struct irc_line *line, gboolean case_sensitive)
{
	return g_strdup("%");
}

static char *get_identifier(struct subst_context *subst_ctx, const struct irc_line *line, gboolean case_sensitive)
{
	if (case_sensitive) return g_ascii_strdown(subst_ctx->identifier, -1);
	else return g_strdup(subst_ctx->identifier);
}

static char *get_modechanges(struct subst_context *subst_ctx, const struct irc_line *line, gboolean case_sensitive)
{
	char buf[512] = "";
	int i;

	for (i = 3 ; i < line->argc && line->args[i] != NULL; i++) {
		if (i > 3) strncat(buf, " ", sizeof(buf)-1);
		strncat(buf, line->args[i], sizeof(buf)-1);
	}

	return g_strdup(buf);
}

static struct log_mapping mappings[] = {
	{NULL, '@', (size_t)-1, get_identifier },
	{NULL, 'h', (size_t)-1, get_hours },
	{NULL, 'M', (size_t)-1, get_minutes },
	{NULL, 's', (size_t)-1, get_seconds },
	{NULL, 'd', (size_t)-1, get_day },
	{NULL, 'B', (size_t)-1, get_month },
	{NULL, 'Y', (size_t)-1, get_year },
	{NULL, 'e', (size_t)-1, get_seconds_since_1970 },
	{NULL, 'b', (size_t)-1, get_monthname },
	{NULL, 'n', (size_t)-1, get_nick },
	{NULL, 'u', (size_t)-1, get_user },
	{NULL, 'N', (size_t)-1, get_network },
	{NULL, 'S', (size_t)-1, get_server },
	{NULL, '%', (size_t)-1, get_percent },
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
	{"MSG", 't', 1, NULL },
	{"MSG", 'm', 2, NULL },
	{"TOPIC", 'c', 1, NULL },
	{"TOPIC", 't', 2, NULL },
	{"MODE", 't', 1, NULL },
	{"MODE", 'p', 2, NULL },
	{"MODE", 'c', (size_t)-1, get_modechanges },
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

/**
 * Given a special character, expand it.
 *
 * @param network IRC Network
 * @param l IRC Line
 * @param c Special character
 * @param case_sensitive Whether or not to be case sensitive
 * @return Expanded string or an empty string if there was no expansion possible
 */
static char *find_mapping(struct subst_context *subst_ctx,
						  const struct irc_line *l,
						  char c, gboolean case_sensitive)
{
	int i;
	for (i = 0; mappings[i].subst; i++) {
		if (mappings[i].command && strcmp(mappings[i].command, l->args[0]))
			continue;

		if (mappings[i].subst != c)
			continue;

		if (mappings[i].index == -1)
			return mappings[i].callback(subst_ctx, l, case_sensitive);

		if (mappings[i].index < l->argc) {
			if (case_sensitive)
				return g_ascii_strdown(l->args[mappings[i].index], -1);
			else
				return g_strdup(l->args[mappings[i].index]);
		}
	}
	return g_strdup("");
}

void convertslashes(char *a)
{
	while (*a) {
		if (*a == '/')
			*a = '_';
		a++;
	}
}

/**
 * Substitute the special characters in a string.
 *
 * @param network IRC Network
 * @param _new Pointer to store expanded line
 * @param fmt String to expand
 * @param l IRC Line
 * @param case_sensitive Whether or not to be case sensitive
 * @param noslash Whether or not to avoid adding slashes from expansions
 */
char *custom_subst(struct irc_network *network,
						 const char *fmt, const struct irc_line *l,
						 const char *_identifier,
						 gboolean case_sensitive, gboolean noslash)
{
	char **subst;
	char *new;
	size_t len, exp_len, curpos = 0;
	unsigned int i;
	struct subst_context subst_ctx;

	subst_ctx.identifier = _identifier;
	subst_ctx.network = network;

	/* First, find all the mappings */
	exp_len = len = strlen(fmt);
	subst = g_new0(char *, len);
	for (i = 0; i < len; i++) {
		if (fmt[i] == '%') {
			subst[i] = find_mapping(&subst_ctx, l, fmt[i+1],
												case_sensitive);	
			if (subst[i] == NULL)
				subst[i] = g_strdup("");
			if (noslash) convertslashes(subst[i]);
			exp_len += strlen(subst[i]);
		}
	}

	exp_len++; /* newline! */

	new = g_new(char, exp_len);
	for (i = 0; i < len; i++) {
		if (fmt[i] == '%') {
			new[curpos] = '\0';
			strncat(new, subst[i], exp_len);
			curpos+=strlen(subst[i]);
			i++;
		} else {
			new[curpos] = fmt[i];
			curpos++;
		}
	}
	new[curpos] = '\0';

	for (i = 0; i < len; i++)
		g_free(subst[i]);
	g_free(subst);
	return new;
}
