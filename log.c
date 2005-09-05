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

static const char *get_date(void)
{
	static char ret[40];
	time_t t = time(NULL);
	strftime(ret, sizeof(ret), "%F %H:%M", localtime(&t));
	return ret;
}

enum log_level current_log_level = LOG_INFO;
FILE *flog;

static void log_entry(const char *module, enum log_level level, const struct network *n, const struct client *c, const char *data)
{
	if (level > current_log_level)
		return;
	fprintf(flog, "[%s] [%s] %s%s%s%s%s%s\n", get_date(), 
			module?module:"core", data, n?" (":"", n?n->name:"", c?"/":"", c?c->description:"", n?")":"");
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

static void log_handler(const gchar *log_domain, GLogLevelFlags flags, const gchar *message, gpointer user_data) {
	log_global(log_domain, LOG_ERROR, message);
}

gboolean init_log(const char *lf)
{
	g_log_set_handler ("GLib", G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION, log_handler, NULL);

	if (!lf) {
		flog = stdout;
		return TRUE;
	}

	flog = fopen(lf, "a+");
	if (!flog) {
		perror("Opening log file");
		return FALSE;
	}

	log_global(NULL, LOG_INFO, "Opening log file");
	return TRUE;
}

void fini_log(void)
{
	log_global(NULL, LOG_INFO, "Closing log file");
	if (flog != stdout) {
		fclose(flog);
	}
}
