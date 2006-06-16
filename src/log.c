/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2005 Jelmer Vernooij <jelmer@nl.linux.org>

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

#include "internals.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdarg.h>
#include <stdio.h>

static const char *get_date(void)
{
	static char ret[40];
	time_t t = time(NULL);
	strftime(ret, sizeof(ret), "%F %H:%M", localtime(&t));
	return ret;
}

gboolean no_log_timestamp = FALSE;
enum log_level current_log_level = LOG_INFO;
FILE *flog = NULL;

static void log_entry(const char *module, enum log_level level, const struct network *n, const struct client *c, const char *data)
{
	if (flog == NULL)
		return;

	if (level > current_log_level)
		return;

	admin_log(module, level, n, c, data);
	
	if (!no_log_timestamp)
		fprintf(flog, "[%s] ", get_date());
	
	if (module) 
		fprintf(flog, "[%s] ", module);

	fprintf(flog, "%s", data);

	if (n) {
		fprintf(flog, " (%s", n->name);

		if (c)
			fprintf(flog, "/%s", c->description);

		fprintf(flog, ")");
	}

	fprintf(flog, "\n");
	fflush(flog);
}

void log_network_line(const struct network *n, const struct line *l, gboolean incoming)
{
	char *raw;
	if (current_log_level < LOG_DATA)
		return;

	raw = irc_line_string(l);
	log_network(NULL, LOG_DATA, n, "%c %s", incoming?'<':'>', raw);
	g_free(raw);
}

void log_client_line(const struct client *n, const struct line *l, gboolean incoming)
{
	char *raw;
	if (current_log_level < LOG_DATA)
		return;

	raw = irc_line_string(l);
	log_client(NULL, LOG_DATA, n, "%c %s", incoming?'<':'>', raw);
	g_free(raw);
}

void log_network(const char *module, enum log_level level, const struct network *n, const char *fmt, ...)
{
	va_list ap;	
	char *tmp; 
	va_start(ap, fmt);
	tmp = g_strdup_vprintf(fmt, ap);
	log_entry(module, level, n, NULL, tmp);
	va_end(ap);
	g_free(tmp);
}

void log_client(const char *module, enum log_level level, const struct client *c, const char *fmt, ...)
{
	va_list ap;	
	char *tmp; 
	va_start(ap, fmt);
	tmp = g_strdup_vprintf(fmt, ap);
	va_end(ap);
	log_entry(module, level, c->network, c, tmp);
	g_free(tmp);
}

void log_global(const char *module, enum log_level level, const char *fmt, ...)
{
	va_list ap;	
	char *tmp; 
	va_start(ap, fmt);
	tmp = g_strdup_vprintf(fmt, ap);
	va_end(ap);
	log_entry(module, level, NULL, NULL, tmp);
	g_free(tmp);
}

void log_network_state(const char *module, enum log_level l, const struct network_state *st, const char *fmt, ...)
{
	char *ret;
	va_list ap;

	g_assert(st);
	g_assert(fmt);

	va_start(ap, fmt);
	ret = g_strdup_vprintf(fmt, ap);
	va_end(ap);

	log_global(module, l, "%s", ret);

	g_free(ret);
}



static void log_handler(const gchar *log_domain, GLogLevelFlags flags, const gchar *message, gpointer user_data) {
	log_global(log_domain, LOG_ERROR, message);
}

static void fini_log(void)
{
	log_global(NULL, LOG_INFO, "Closing log file");
	if (flog != stderr) {
		fclose(flog);
	}
	flog = NULL;
}

gboolean init_log(const char *lf)
{
	g_log_set_handler ("GLib", G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION, log_handler, NULL);

	if (!lf) {
		flog = stderr;
		return TRUE;
	}

	flog = fopen(lf, "a+");
	if (!flog) {
		perror("Opening log file");
		return FALSE;
	}

	atexit(fini_log);

	log_global(NULL, LOG_INFO, "Opening log file");
	return TRUE;
}