/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2005 Jelmer Vernooij <jelmer@jelmer.uk>

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

static const char *get_date(void)
{
	static char ret[40];
	time_t t = time(NULL);
	strftime(ret, sizeof(ret), "%F %H:%M:%S", localtime(&t));
	return ret;
}

gboolean no_log_timestamp = FALSE;
enum log_level current_log_level = LOG_INFO;
FILE *flog = NULL;

void log_entry(enum log_level level, const struct irc_network *n, const struct irc_client *c, const char *data)
{
	if (flog == NULL)
		return;

	if (level > current_log_level)
		return;

	if (level < LOG_DATA)
		admin_log(level, n, c, data);
	
	if (!no_log_timestamp)
		fprintf(flog, "[%s] ", get_date());
	
	g_assert(strchr(data, '\n') == NULL);
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

void log_network_line(const struct irc_network *n, const struct irc_line *l, gboolean incoming)
{
	char *raw;
	if (current_log_level < LOG_DATA)
		return;

	raw = irc_line_string(l);
	network_log(LOG_DATA, n, "%c %s", incoming?'<':'>', raw);
	g_free(raw);
}

void log_client_line(const struct irc_client *n, const struct irc_line *l, gboolean incoming)
{
	char *raw;
	if (current_log_level < LOG_DATA)
		return;

	raw = irc_line_string(l);
	client_log(LOG_DATA, n, "%c %s", incoming?'<':'>', raw);
	g_free(raw);
}

void handle_network_log(enum log_level level, const struct irc_network *n, 
						const char *msg)
{
	g_assert(n != NULL);
	log_entry(level, n, NULL, msg);
	if (level <= LOG_INFO)
		clients_send_args_ex(n->clients, NULL, "NOTICE", "*", 
						     msg, NULL);
}

void log_client(enum log_level level, const struct irc_client *c, const char *data)
{
	log_entry(level, c->network, c, data);
}

void log_global(enum log_level level, const char *fmt, ...)
{
	va_list ap;	
	char *tmp; 
	va_start(ap, fmt);
	tmp = g_strdup_vprintf(fmt, ap);
	va_end(ap);
	log_entry(level, NULL, NULL, tmp);
	g_free(tmp);
}

static void log_handler(const gchar *log_domain, GLogLevelFlags flags, const gchar *message, gpointer user_data) 
{
	if (strchr(message, '\n')) {
		char **lines = g_strsplit(message, "\n", 0);
		int i;
		for (i = 0; lines[i]; i++) {
			if (strlen(lines[i]) == 0)
				continue;
			log_global(LOG_ERROR, "[%s] %s", log_domain, lines[i]);
		}
		g_strfreev(lines);
	} else {
		log_global(LOG_ERROR, "[%s] %s", log_domain, message);
	}
}

static void fini_log(void)
{
	log_global(LOG_INFO, "Closing log file");
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

	log_global(LOG_INFO, "Opening log file");
	return TRUE;
}
