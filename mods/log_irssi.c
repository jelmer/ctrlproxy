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

static char *logbasedir = NULL;
struct log_support_context *log_ctx = NULL;

void strip_slashes(char *cn)
{
	int i;
	for (i = 0; cn[i]; i++) 
		if (cn[i] == '/') 
			cn[i] = '_';

}

static void target_vprintf(struct network *s, const char *name, 
						   char *fmt, va_list ap)
{
	char *lowercase;
	char *text;
	char *hash_name;

	lowercase = g_ascii_strdown(name != NULL?name:"messages", -1);
	strip_slashes(lowercase);
	hash_name = g_build_filename(logbasedir, s->info.name, lowercase, NULL);
	g_free(lowercase);

	/* Then open the correct filename */
	text = g_strdup_vprintf(fmt, ap);
	log_support_write(log_ctx, hash_name, text);
	g_free(text);

	g_free(hash_name);
}

static void target_printf(struct network *n, const char *name, 
						  char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	if (strchr(name, ',') != NULL) {
		char **channels = g_strsplit(name, ",", 0);
		int i;

		for (i = 0; channels[i]; i++) {
			target_vprintf(n, channels[i], fmt, ap);
		}
		g_strfreev(channels);
	} else 
		target_vprintf(n, name, fmt, ap);
	va_end(ap);
}


static gboolean log_data(struct network *n, const struct line *l, 
						 enum data_direction dir, void *userdata)
{
	char *nick = NULL;
	const char *dest = NULL;
	time_t ti = time(NULL);
	char *user = NULL;
	struct tm *t = localtime(&ti);
	
	if (l->args == NULL || l->args[0] == NULL)
		return TRUE;

	if (l->origin != NULL) {
		nick = line_get_nick(l);
		user = strchr(l->origin, '!');
		if (user != NULL) 
			user++;
	}

	if (dir == FROM_SERVER && !g_strcasecmp(l->args[0], "JOIN")) {
		target_printf(n, l->args[1], "%02d:%02d -!- %s [%s] has joined %s\n", t->tm_hour, t->tm_min, nick, user, l->args[1]);
	} else if (dir == FROM_SERVER && !g_strcasecmp(l->args[0], "PART")) {
		target_printf(n, l->args[1], "%02d:%02d -!- %s [%s] has left %s [%s]\n", t->tm_hour, t->tm_min, nick, user, l->args[1], l->args[2]?l->args[2]:"");
	} else if (!g_strcasecmp(l->args[0], "PRIVMSG") && l->argc > 2) {
		dest = l->args[1];
		if (!irccmp(&n->state->info, dest, n->state->me.nick)) dest = nick;
		if (l->args[2][0] == '\001') { 
			l->args[2][strlen(l->args[2])-1] = '\0';
			if (!g_ascii_strncasecmp(l->args[2], "\001ACTION ", 8)) { 
				target_printf(n, dest, "%02d:%02d  * %s %s\n", t->tm_hour, t->tm_min, nick, l->args[2]+8);
			}
			l->args[2][strlen(l->args[2])] = '\001';
			/* Ignore all other ctcp messages */
		} else {
			target_printf(n, dest, "%02d:%02d < %s> %s\n", t->tm_hour, t->tm_min, nick, l->args[2]);
		}
	} else if (!g_strcasecmp(l->args[0], "MODE") && l->args[1] && 
			  is_channelname(l->args[1], &n->state->info) && dir == FROM_SERVER) {
		target_printf(n, l->args[1], "%02d:%02d -!- mode/%s [%s %s] by %s\n", t->tm_hour, t->tm_min, l->args[1], l->args[2], l->args[3], nick);
	} else if (!g_strcasecmp(l->args[0], "QUIT")) {
		/* Loop thru the channels this user is on */
		GList *gl;
		struct network_nick *nn = find_network_nick(n->state, nick);
		if (nn != NULL) {
			for (gl = nn->channel_nicks; gl; gl = gl ->next) {
				struct channel_nick *cn = (struct channel_nick *)gl->data;
				target_printf(n, cn->channel->name, "%02d:%02d -!- %s [%s] has quit [%s]\n", t->tm_hour, t->tm_min, nick, user, l->args[1]?l->args[1]:"");
			}
		}
	} else if (!g_strcasecmp(l->args[0], "KICK") && l->args[1] && l->args[2] && dir == FROM_SERVER) {
		if (strchr(l->args[1], ',') == NULL) {
			target_printf(n, l->args[1], "%02d:%02d -!- %s has been kicked by %s [%s]\n", t->tm_hour, t->tm_min, l->args[2], nick, l->args[3]?l->args[3]:"");
		} else { 
			char *channels = g_strdup(l->args[1]);
			char *nicks = g_strdup(l->args[1]);
			char *p,*nx; 
			gboolean cont = TRUE;
			char *_nick;

			p = channels;
			_nick = nicks;
			while (cont) {
				nx = strchr(p, ',');

				if (nx == NULL) 
					cont = FALSE;
				else 
					*nx = '\0';

				target_printf(n, p, "%02d:%02d -!- %s has been kicked by %s [%s]\n", t->tm_hour, t->tm_min, _nick, nick, l->args[3]?l->args[3]:"");

				p = nx+1;
				_nick = strchr(_nick, ',');
				if (_nick == NULL)
					break;
				_nick++;
			}
			
			g_free(channels);
			g_free(nicks);
		}
	} else if (!g_strcasecmp(l->args[0], "TOPIC") && dir == FROM_SERVER && l->args[1]) {
		if (l->args[2] != NULL)
			target_printf(n, l->args[1], "%02d:%02d -!- %s has changed the topic to %s\n", t->tm_hour, t->tm_min, nick, l->args[2]);
		else 
			target_printf(n, l->args[1], "%02d:%02d -!- %s has removed the topic\n", t->tm_hour, t->tm_min, nick);
	} else if (!g_strcasecmp(l->args[0], "NICK") && 
			   dir == FROM_SERVER && l->args[1]) {
		struct network_nick *nn = find_network_nick(n->state, nick);
		GList *gl;

		if (nn != NULL) {
			for (gl = nn->channel_nicks; gl; gl = gl->next) {
				struct channel_nick *cn = gl->data;

				target_printf(n, cn->channel->name, "%02d:%02d -!- %s is now known as %s\n", t->tm_hour, t->tm_min, nick, l->args[1]);
			}
		}
	}

	g_free(nick);

	return TRUE;
}

static void load_config(struct global *global)
{
	GKeyFile *kf;
	
	kf = global->config->keyfile;
	if (!g_key_file_has_group(kf, "log-irssi")) {
		del_log_filter("log_irssi");
		return;
	}

	if (!g_key_file_has_key(kf, "log-irssi", "logfile", NULL)) {
		logbasedir = g_build_filename(global->config->config_dir, "log_irssi", NULL);
	} else {
		logbasedir = g_key_file_get_string(kf, "log-irssi", "logfile", NULL);
	}
	

	log_ctx = log_support_init();

	/* Create logfile directory if it doesn't exist yet */
	add_log_filter("log_irssi", log_data, NULL, 1000);
}

static gboolean init_plugin()
{
	register_load_config_notify(load_config);
	return TRUE;
}

struct plugin_ops plugin = {
	.name = "log_irssi",
	.version = CTRLPROXY_PLUGIN_VERSION,
	.init = init_plugin
};
