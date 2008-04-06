/* 
	ctrlproxy: A modular IRC proxy
	(c) 2002-2003 Jelmer Vernooij <jelmer@nl.linux.org>

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

char *get_ctcp_command(const char *data)
{
	char *tmp;
	char *p;
	g_assert(data[0] == '\001');

	tmp = g_strdup(data+1);

	p = strchr(tmp, ' ');
	if (p) {
		*p = '\0';
		return tmp;
	}

	p = strrchr(tmp, '\001');
	if (p) {
		*p = '\0';
		return tmp;
	}

	g_free(tmp);
	return NULL;
}

/**
 * A CTCP request.
 */
struct ctcp_request {
	struct irc_client *client;
	char *destination;
	char *command;
};

static GList *ctcp_request_queue = NULL;

gboolean ctcp_client_request_record (struct irc_client *c, struct irc_line *l)
{
	struct ctcp_request *req;
	char *command = get_ctcp_command(l->args[2]);

	if (command == NULL) {
		client_log(LOG_WARNING, c, "Received mailformed CTCP request");
		return FALSE;
	}

	req = g_new0(struct ctcp_request, 1);

	/* store client and command in table */
	req->client = c;
	if (!is_channelname(l->args[1], c->network->info))
		req->destination = g_strdup(l->args[1]);
	req->command = command;

	client_log(LOG_TRACE, c, "Tracking CTCP request '%s' to %s", req->command, req->destination);

	ctcp_request_queue = g_list_append(ctcp_request_queue, req);

	return TRUE;
}

gboolean ctcp_network_redirect_response(struct irc_network *n, const struct irc_line *l) 
{
	GList *gl;
	char *nick;

	char *command = get_ctcp_command(l->args[2]);

	nick = line_get_nick(l);

	if (command == NULL) {
		network_log(LOG_WARNING, n, "Received mailformed CTCP request from `%s'", nick);
		g_free(nick);
		return FALSE;
	}


	/* look in table */
	/* if found send to specified client, remove entry from table */
	for (gl = ctcp_request_queue; gl; gl = gl->next) {
		struct ctcp_request *req = gl->data;

		if (req->client->network != n)
			continue;

		if (req->destination && strcmp(req->destination, nick) != 0)
			continue;

		if (strcmp(req->command, command) != 0)
			continue;

		client_send_line(req->client, l);

		g_free(req->command);
		g_free(req->destination);
		ctcp_request_queue = g_list_remove(ctcp_request_queue, req);
		g_free(req);
		g_free(nick);
		g_free(command);

		return TRUE;
	}

	/* otherwise, inform user */
	network_log(LOG_WARNING, n, "Don't know where to send unknown CTCP reply '%s'", command);

	g_free(command);

	return TRUE;
}
