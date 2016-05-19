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

static struct irc_network_info *network_get_info(struct irc_network *network)
{

	if (network->external_state == NULL || network->external_state->info == NULL) {
		return NULL;
	} else {
		return network->external_state->info;
	}
}

struct log_custom_data {
	struct log_file_config *config;
	struct log_support_context *log_ctx;
};

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

static void file_write_line(struct log_custom_data *data, struct irc_network *network,
			    const char *fmt, const struct irc_line *l, const
			    char *identifier)
{
	char *s;
	char *n = NULL;
	char *line;

	if (data->config->logfilename == NULL) {
		return;
	}

	s = custom_subst(network, fmt, l, identifier, FALSE, FALSE);

	n = custom_subst(network, data->config->logfilename, l, identifier,
			 TRUE, TRUE);

	line = g_strdup_printf("%s\n", s);
	g_free(s);

	log_support_write(data->log_ctx, n, line);

	g_free(line);

	g_free(n);
}

static void file_write_line_target(struct log_custom_data *data,
				   struct irc_network *network, const char *fmt,
				   const struct irc_line *l, const char *t)
{
	if (strchr(t, ',') != NULL) {
		char **channels = g_strsplit(t, ",", 0);
		int i;

		for (i = 0; channels[i]; i++) {
			file_write_line(data, network, fmt, l, channels[i]);
		}
		g_strfreev(channels);
	} else {
		file_write_line(data, network, fmt, l, t);
	}
}

static void file_write_target(struct log_custom_data *data,
			      struct irc_network *network, const char *fmt,
			      const struct irc_line *l)
{
	char *t;
	struct irc_network_info *info;

	if (fmt == NULL)
		return;

	g_assert(l->args[0] != NULL);
	g_assert(l->args[1] != NULL);
	info = network_get_info(network);
	g_assert(network->external_state->me.nick != NULL);

	if (network->external_state != NULL &&
		network->external_state->me.nick != NULL &&
		!irccmp(info, network->external_state->me.nick, l->args[1])) {
		if (l->origin != NULL)
			t = line_get_nick(l);
		else
			t = g_strdup("_messages_");
		file_write_line(data, network, fmt, l, t);
		g_free(t);
	} else
		file_write_line_target(data, network, fmt, l, l->args[1]);
}

static void file_write_channel_only(struct log_custom_data *data,
				    struct irc_network *network,
				    const char *fmt, const struct irc_line *l)
{
	if (fmt == NULL)
		return;

	file_write_line_target(data, network, fmt, l, l->args[1]);
}

static void file_write_channel_query(struct log_custom_data *data,
				     struct irc_network *network,
				     const char *fmt,
				     const struct irc_line *l)
{
	char *nick;
	GList *gl;
	struct network_nick *nn;

	if (l->origin == NULL) {
		return;
	}

	if (fmt == NULL) {
		return;
	}

	if (network->external_state == NULL) {
		return;
	}

	/* check for the query first */
	nick = line_get_nick(l);

	nn = find_network_nick(network->external_state, nick);
	if (nn == NULL) {
		network_log(LOG_WARNING, network,
			    "Unable to find query with %s", nick);
		g_free(nick);
		return;
	}

	if (nn->query) {
		file_write_line(data, network, fmt, l, nick);
	}

	g_free(nick);

	/* now, loop thru the users' channels */
	for (gl = nn->channel_nicks; gl; gl = gl->next) {
		struct channel_nick *cn = gl->data;
		file_write_line(data, network, fmt, l, cn->channel->name);
	}
}

static gboolean log_custom_data(struct irc_network *network,
				const struct irc_line *l,
				enum data_direction dir, void *userdata)
{
	struct log_custom_data *data = userdata;
	char *nick = NULL;
	if (l->args == NULL || l->args[0] == NULL) {
		return TRUE;
	}

	if (l->origin != NULL) {
		nick = line_get_nick(l);
	}

	/* Loop thru possible values for %@ */

	/* There are a few possibilities:
	 * - log to line_get_nick(l) or l->args[1] file depending on which
	 *   was the current user (PRIVMSG, NOTICE, ACTION, MODE) (target)
	 * - log to all channels line_get_nick(l) was on, and to query, if
	 *   applicable (NICK, QUIT) (channel_query)
	 * - log to channel only (KICK, PART, JOIN, TOPIC) (channel_only)
	 */

	if (dir == FROM_SERVER && !g_strcasecmp(l->args[0], "JOIN")) {
		file_write_target(data, network, data->config->join, l);
	} else if (dir == FROM_SERVER && !g_strcasecmp(l->args[0], "PART")) {
		file_write_channel_only(data, network, data->config->part, l);
	} else if (!g_strcasecmp(l->args[0], "PRIVMSG") && l->args[2] != NULL) {
		if (l->args[2][0] == '\001') {
			l->args[2][strlen(l->args[2])-1] = '\0';
			if (!g_ascii_strncasecmp(l->args[2], "\001ACTION ", 8)) {
				l->args[2]+=8;
				file_write_target(data, network, data->config->action, l);
				l->args[2]-=8;
			}
			l->args[2][strlen(l->args[2])] = '\001';
			/* Ignore all other ctcp messages */
		} else {
			file_write_target(data, network, data->config->msg, l);
		}
	} else if (!g_strcasecmp(l->args[0], "NOTICE")) {
		file_write_target(data, network, data->config->notice, l);
	} else if (!g_strcasecmp(l->args[0], "MODE") && l->args[1] != NULL &&
			  is_channelname(l->args[1], network_get_info(network)) &&
			  dir == FROM_SERVER) {
		file_write_target(data, network, data->config->mode, l);
	} else if (!g_strcasecmp(l->args[0], "QUIT")) {
		file_write_channel_query(data, network, data->config->quit, l);
	} else if (!g_strcasecmp(l->args[0], "KICK") && l->args[1] != NULL &&
			   l->args[2] != NULL && dir == FROM_SERVER) {
		if (strchr(l->args[1], ',') == NULL) {
			file_write_channel_only(data, network, data->config->kick, l);
		} else {
			char *channels = g_strdup(l->args[1]);
			char *nicks = g_strdup(l->args[1]);
			char *p,*n; gboolean cont = TRUE;
			char *_nick;

			p = channels;
			_nick = nicks;
			while (cont) {
				n = strchr(p, ',');

				if (n == NULL)
					cont = FALSE;
				else
					*n = '\0';

				file_write_channel_only(data, network, data->config->kick, l);

				p = n+1;
				_nick = strchr(_nick, ',');
				if (!_nick)break;
				_nick++;
			}

			g_free(channels);
			g_free(nicks);
		}
	} else if (!g_strcasecmp(l->args[0], "TOPIC") && dir == FROM_SERVER &&
		l->args[1] != NULL) {
		if (l->args[2] != NULL) {
			file_write_channel_only(data, network, data->config->topic, l);
		} else {
			file_write_channel_only(data, network, data->config->notopic, l);
		}
	} else if (!g_strcasecmp(l->args[0], "NICK") && dir == FROM_SERVER &&
			   l->args[1] != NULL) {
		file_write_channel_query(data, network, data->config->nickchange, l);
	}

	g_free(nick);

	return TRUE;
}

void log_custom_load(struct log_file_config *config)
{
	struct log_custom_data *data = g_new0(struct log_custom_data, 1);
	data->config = config;
	data->log_ctx = log_support_init();
	add_log_filter("log_custom", log_custom_data, data, 1000);
	register_hup_handler((hup_handler_fn)log_support_reopen, data->log_ctx);
}
