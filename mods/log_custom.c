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
#include <glib/gstdio.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_SUBST 256

struct file_info {
	FILE *file;
	time_t last_used;
};

struct log_custom_data {
    char *logfilename;
    GKeyFile *kf;
};

static GHashTable *files;

/* Translation table */
struct log_mapping {
	char *command;
	char subst;
	unsigned int index;
	/* If index is -1 */
	char *(*callback) (struct network *, const struct line *l, 
					   gboolean case_sensitive);
};

static char *get_hours(struct network *n, const struct line *l, 
					   gboolean case_sensitive) 
{ 
	time_t ti = time(NULL);
	struct tm *t = localtime(&ti);
	return g_strdup_printf("%02d", t->tm_hour);
}

static char *get_minutes(struct network *n, const struct line *l, 
						 gboolean case_sensitive) 
{ 
	time_t ti = time(NULL);
	struct tm *t = localtime(&ti);
	return g_strdup_printf("%02d", t->tm_min);
}

static char *get_seconds(struct network *n, const struct line *l, 
						 gboolean case_sensitive) 
{ 
	time_t ti = time(NULL);
	struct tm *t = localtime(&ti);
	return g_strdup_printf("%02d", t->tm_sec);
}

static char *get_seconds_since_1970(struct network *n, const struct line *l, 
									gboolean case_sensitive) 
{
	time_t ti = time(NULL);
	return g_strdup_printf("%ld", ti);
}

static char *get_day(struct network *n, const struct line *l, 
					 gboolean case_sensitive) 
{ 
	time_t ti = time(NULL);
	struct tm *t = localtime(&ti);
	return g_strdup_printf("%02d", t->tm_mday);
}

static char *get_month(struct network *n, const struct line *l, 
					   gboolean case_sensitive) 
{ 
	time_t ti = time(NULL);
	struct tm *t = localtime(&ti);
	return g_strdup_printf("%02d", t->tm_mon + 1);
}

static char *get_year(struct network *n, const struct line *l, 
					  gboolean case_sensitive) 
{ 
	time_t ti = time(NULL);
	struct tm *t = localtime(&ti);
	return g_strdup_printf("%04d", t->tm_year + 1900);
}

static char *get_user(struct network *n, const struct line *l, 
					  gboolean case_sensitive) 
{
	char *nick = NULL;
	char *user = NULL;

	if (l->origin != NULL) nick = g_strdup(l->origin);
	if (nick != NULL) user = strchr(nick, '!');
	if (user != NULL) { *user = '\0';user++; }

	if (case_sensitive) return g_ascii_strdown(user, -1);
	else return g_strdup(user);
}

static char *get_monthname(struct network *n, const struct line *l, 
						   gboolean case_sensitive) 
{ 
	char stime[512];
	time_t ti = time(NULL);
	strftime(stime, sizeof(stime), "%b", localtime(&ti));
	return g_strdup_printf("%s", stime);
}

static char *get_nick(struct network *n, const struct line *l, 
					  gboolean case_sensitive) 
{
	if (l->origin != NULL) {
		char *n = line_get_nick(l);
		if (case_sensitive) {
			char *r = g_ascii_strdown(n, -1);
			g_free(n);
			return r;
		}
		else return n;
	}
	
	return g_strdup("");
}

static char *get_network(struct network *n, const struct line *l, 
						 gboolean case_sensitive) 
{
	return g_strdup(n->info.name); 
}

static char *get_server(struct network *n, const struct line *l, 
						gboolean case_sensitive)
{
	if (n->connection.data.tcp.current_server)
		return g_strdup(n->connection.data.tcp.current_server->host);
	return g_strdup("");
}

static char *get_percent(struct network *n, const struct line *l, 
						 gboolean case_sensitive) 
{ 
	return g_strdup("%"); 
}

static const char *identifier = NULL;

static char *get_identifier(struct network *n, const struct line *l, 
							gboolean case_sensitive) 
{ 
	if (case_sensitive) return g_ascii_strdown(identifier, -1); 
	else return g_strdup(identifier); 
}

static char *get_modechanges(struct network *n, const struct line *l, 
							 gboolean case_sensitive) 
{
	char buf[512] = "";
	int i;

	for (i = 3 ; l->args[i+1] != NULL; i++)
		if (i == 3) sprintf(buf, "%s", l->args[i]);
		else sprintf(buf, "%s %s", buf, l->args[i]);

	return g_strdup(buf);
}

static struct log_mapping mappings[] = {
	{NULL, '@', -1, get_identifier },
	{NULL, 'h', -1, get_hours },
	{NULL, 'M', -1, get_minutes },
	{NULL, 's', -1, get_seconds },
	{NULL, 'd', -1, get_day },
	{NULL, 'B', -1, get_month },
	{NULL, 'Y', -1, get_year },
	{NULL, 'e', -1, get_seconds_since_1970 },
	{NULL, 'b', -1, get_monthname },
	{NULL, 'n', -1, get_nick },
	{NULL, 'u', -1, get_user },
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
	{"MSG", 't', 1, NULL },
	{"MSG", 'm', 2, NULL },
	{"TOPIC", 'c', 1, NULL },
	{"TOPIC", 't', 2, NULL },
	{"MODE", 't', 1, NULL },
	{"MODE", 'p', 2, NULL },
	{"MODE", 'c', -1, get_modechanges },
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

static char *find_mapping(struct network *network, const struct line *l, char c, gboolean case_sensitive)
{
	int i;
	for (i = 0; mappings[i].subst; i++) {
		if (mappings[i].command && 
		   strcmp(mappings[i].command, l->args[0])) continue;
		if (mappings[i].subst != c) continue;
		if (mappings[i].index == -1) return mappings[i].callback(network, l, case_sensitive);
		if (mappings[i].index < l->argc) {
			if (case_sensitive) return g_ascii_strdown(l->args[mappings[i].index], -1);
			else return g_strdup(l->args[mappings[i].index]);
		}
	}
	return g_strdup("");
}

static void convertslashes(char *a)
{
	int j;
	for (j = 0; a[j]; j++) 
		if (a[j] == '/') a[j] = '_';
}

static void custom_subst(struct network *network, char **_new, 
						 const char *fmt, const struct line *l, 
						 const char *_identifier, 
						 gboolean case_sensitive, gboolean noslash)
{
	char *subst[MAX_SUBST];
	char *new;
	size_t len, curpos = 0;
	unsigned int i;

	identifier = _identifier;

	/* First, find all the mappings */
	len = strlen(fmt);
	memset(subst, 0, sizeof(char *) * MAX_SUBST);
	for (i = 0; i < strlen(fmt); i++) {
		if (fmt[i] == '%') {
			subst[(int)fmt[i+1]] = find_mapping(network, l, fmt[i+1], case_sensitive);	
			if (subst[(int)fmt[i+1]] == NULL) subst[(int)fmt[i+1]] = g_strdup("");
			if (noslash) convertslashes(subst[(int)fmt[i+1]]);
			len += strlen(subst[(int)fmt[i+1]]);
		}
	}

	len++; /* newline! */

	new = g_new(char, len);
	for (i = 0; i < strlen(fmt); i++) {
		if (fmt[i] == '%') {
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

	for (i = 0; i < MAX_SUBST; i++) { 
		if (subst[i]) 
			g_free(subst[i]); 
	}
	*_new = new;
}

/* Syntax:
Always:
 * %@ -> identifier
 * %h -> hours
 * %M -> minutes
 * %s -> seconds
 * %d -> day
 * %B -> month
 * %Y -> year
 * %e -> seconds since 1970
 * %b -> locale month name
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
 -- MODE: %p(mode change), %t, %c (target nicks)
 -- TOPIC: %t
 -- NICK: %r
 */

static FILE *find_add_channel_file(struct log_custom_data *data, 
								   struct network *network, 
								   const struct line *l, 
								   const char *identifier, 
								   gboolean create_file) 
{
	char *n = NULL, *dn, *p;
	struct file_info *fi;
	if (data->logfilename == NULL) 
		return NULL;

	custom_subst(network, &n, data->logfilename, l, identifier, TRUE, TRUE);
	fi = g_hash_table_lookup(files, n);

	if (fi == NULL && create_file) {
		dn = g_strdup(n);
		
		/* Only include directory-part */
		p = strrchr(dn, '/');
		if (p != NULL) 
			*p = '\0';

		/* Check if directory needs to be created */
		if (!g_file_test(dn, G_FILE_TEST_IS_DIR) && g_mkdir(dn, 0700) == -1) {
			log_network(LOG_ERROR, network, "Couldn't create directory %s for logging!", dn);
			g_free(dn);
			g_free(n);
			return NULL;
		}
		g_free(dn);

		fi = g_new0(struct file_info, 1);
		
		/* Then open the correct filename */
		fi->file = fopen(n, "a+");
		if (fi->file == NULL) {
			log_network(LOG_ERROR, network, "Couldn't open file %s for logging!", n);
			g_free(n);
			g_free(fi);
			return NULL;
		}
		g_hash_table_insert(files, n, fi);
	} else g_free(n);

	if (fi == NULL)
		return NULL;

	fi->last_used = time(NULL);
	return fi->file;
}

static void file_write_line(struct log_custom_data *data, struct network *network, const char *fmt,
				   const struct line *l, const char *t, gboolean create_file)
{
	FILE *f;
	char *s;

	f = find_add_channel_file(data, network, l, t, create_file);
	if (f == NULL) return;

	custom_subst(network, &s, fmt, l, t, FALSE, FALSE);

	fputs(s, f);
	fputc('\n', f);
	fflush(f);

	g_free(s);
}

static void file_write_line_target(struct log_custom_data *data, struct network *network, const char *fmt,
				   const struct line *l, const char *t, gboolean create_file)
{
	if (strchr(t, ',') != NULL) {
		char **channels = g_strsplit(t, ",", 0);
		int i;

		for (i = 0; channels[i]; i++) {
			file_write_line(data, network, fmt, l, channels[i], TRUE);
		}
		g_strfreev(channels);
	} else
		file_write_line(data, network, fmt, l, t, TRUE);
}

static void file_write_target(struct log_custom_data *data, 
							  struct network *network, const char *n, 
							  const struct line *l) 
{
	char *t, *fmt;
	
	fmt = g_key_file_get_string(data->kf, "log-custom", n, NULL);
	if (fmt == NULL) 
		return;
	
	g_assert(l->args[0]);
	g_assert(l->args[1]);
	g_assert(network->state);
	g_assert(network->state->me.nick);

	if (!irccmp(&network->state->info, network->state->me.nick, l->args[1])) {
		if (l->origin != NULL)
			t = line_get_nick(l);
		else 
			t = g_strdup("_messages_");
		file_write_line(data, network, fmt, l, t, TRUE);
		g_free(t);
	} else
		file_write_line_target(data, network, fmt, l, l->args[1], TRUE);
}

static void file_write_channel_only(struct log_custom_data *data, 
									struct network *network, const char *n, 
									const struct line *l)
{
	char *fmt;

	fmt = g_key_file_get_string(data->kf, "log-custom", n, NULL);
	if (fmt == NULL) return;

	file_write_line_target(data, network, fmt, l, l->args[1], TRUE);
}

static void file_write_channel_query(struct log_custom_data *data, 
									 struct network *network, const char *n, 
									 const struct line *l)
{
	char *fmt;
	char *nick;
	GList *gl;
	struct network_nick *nn;

	if (l->origin == NULL) 
		return;

	g_assert(n);

	fmt = g_key_file_get_string(data->kf, "log-custom", n, NULL);
	if (fmt == NULL) 
		return;

	/* check for the query first */
	nick = line_get_nick(l);
	file_write_line(data, network, fmt, l, nick, FALSE);
	
	nn = find_network_nick(network->state, nick);
	g_free(nick);
	g_assert(nn);

	/* now, loop thru the users' channels */
	for (gl = nn->channel_nicks; gl; gl = gl->next) {
		struct channel_nick *cn = gl->data;
		file_write_line(data, network, fmt, l, cn->channel->name, TRUE);
	}
}

static gboolean log_custom_data(struct network *network, const struct line *l, 
								enum data_direction dir, void *userdata)
{
    struct log_custom_data *data = userdata;
	char *nick = NULL;
	if (l->args == NULL || l->args[0] == NULL)
		return TRUE;

	if (l->origin != NULL) 
		nick = line_get_nick(l);

	/* Loop thru possible values for %@ */

	/* There are a few possibilities:
	 * - log to line_get_nick(l) or l->args[1] file depending on which 
	 *   was the current user (PRIVMSG, NOTICE, ACTION, MODE) (target)
	 * - log to all channels line_get_nick(l) was on, and to query, if applicable (NICK, QUIT) (channel_query)
	 * - log to channel only (KICK, PART, JOIN, TOPIC) (channel_only)
	 */

	if (dir == FROM_SERVER && !g_strcasecmp(l->args[0], "JOIN")) {
		file_write_target(data, network, "join", l); 
	} else if (dir == FROM_SERVER && !g_strcasecmp(l->args[0], "PART")) {
		file_write_channel_only(data, network, "part", l);
	} else if (!g_strcasecmp(l->args[0], "PRIVMSG")) {
		if (l->args[2][0] == '\001') { 
			l->args[2][strlen(l->args[2])-1] = '\0';
			if (!g_ascii_strncasecmp(l->args[2], "\001ACTION ", 8)) { 
				l->args[2]+=8;
				file_write_target(data, network, "action", l);
				l->args[2]-=8;
			}
			l->args[2][strlen(l->args[2])] = '\001';
			/* Ignore all other ctcp messages */
		} else {
			file_write_target(data, network, "msg", l);
		}
	} else if (!g_strcasecmp(l->args[0], "NOTICE")) {
		file_write_target(data, network, "notice", l);
	} else if (!g_strcasecmp(l->args[0], "MODE") && l->args[1] != NULL && 
			  is_channelname(l->args[1], &network->state->info) && 
			  dir == FROM_SERVER) {
		file_write_target(data, network, "mode", l);
	} else if (!g_strcasecmp(l->args[0], "QUIT")) {
		file_write_channel_query(data, network, "quit", l);
	} else if (!g_strcasecmp(l->args[0], "KICK") && l->args[1] && l->args[2] && dir == FROM_SERVER) {
		if (strchr(l->args[1], ',') == NULL) {
			file_write_channel_only(data, network, "kick", l);
		} else { 
			char *channels = g_strdup(l->args[1]);
			char *nicks = g_strdup(l->args[1]);
			char *p,*n; char cont = 1;
			char *_nick;

			p = channels;
			_nick = nicks;
			while (cont) {
				n = strchr(p, ',');

				if (!n) cont = 0;
				else *n = '\0';

				file_write_channel_only(data, network, "kick", l);

				p = n+1;
				_nick = strchr(_nick, ',');
				if (!_nick)break;
				_nick++;
			}
			
			g_free(channels);
			g_free(nicks);
		}
	} else if (!g_strcasecmp(l->args[0], "TOPIC") && dir == FROM_SERVER && l->args[1]) {
		if (l->args[2] != NULL) 
			file_write_channel_only(data, network, "topic", l);
		else 
			file_write_channel_only(data, network, "notopic", l);
	} else if (!g_strcasecmp(l->args[0], "NICK") && dir == FROM_SERVER && 
			   l->args[1] != NULL) {
		file_write_channel_query(data, network, "nickchange", l);
	}

	g_free(nick);

	return TRUE;
}

static void free_file_info(void *_data)
{
	struct file_info *data = _data;

	fclose(data->file);
	g_free(data);
}

static void load_config(struct global *global)
{
	GKeyFile *kf = global->config->keyfile;
	struct log_custom_data *data;

	if (!g_key_file_has_group(kf, "log-custom")) {
		del_log_filter("log_custom");
		return;
	}

	data = g_new0(struct log_custom_data, 1);

	add_log_filter("log_custom", log_custom_data, data, 1000);

	data->logfilename = g_key_file_get_string(kf, "log-custom", "logfilename", NULL);
	data->kf = kf;
}

#define CLEANUP_THRESHOLD (60 * 60 * 24)

static gboolean eval_remove(gpointer key, gpointer value, gpointer user_data)
{
	struct file_info *fi = value;

	return (fi->last_used < time(NULL) - CLEANUP_THRESHOLD);
}

static gboolean cleanup(void *_data)
{
	g_hash_table_foreach_remove(files, eval_remove, NULL);
	return TRUE;
}

static gboolean init_plugin(void)
{
	files = g_hash_table_new_full(g_str_hash, g_str_equal, 
			g_free, free_file_info);
	g_timeout_add(60 * 60, cleanup, NULL);
	register_load_config_notify(load_config);
	return TRUE;
}

struct plugin_ops plugin = {
	.name = "log_custom",
	.version = 0,
	.init = init_plugin,
};
