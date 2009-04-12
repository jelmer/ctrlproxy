/* 
	ctrlproxy: A modular IRC proxy
	Send numerics to the right places
	(c) 2002-2005 Jelmer Vernooij <jelmer@nl.linux.org>

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
#include <string.h>
#include "irc.h"

static gboolean handle_465(struct irc_network *n, const struct irc_line *l)
{
	network_log(LOG_ERROR, n, "Banned from server: %s", l->args[1]);
	return TRUE;
}

static gboolean handle_451(struct irc_network *n, const struct irc_line *l)
{
	network_log(LOG_ERROR, n, "Not registered error, this is probably a bug...");
	return TRUE;
}

static gboolean handle_462(struct irc_network *n, const struct irc_line *l)
{
	network_log(LOG_ERROR, n, "Double registration error, this is probably a bug...");
	return TRUE;
}

static gboolean handle_463(struct irc_network *n, const struct irc_line *l)
{
	network_log(LOG_ERROR, n, "Host not privileged to connect");
	return TRUE;
}

static gboolean handle_464(struct irc_network *n, const struct irc_line *l)
{
	network_log(LOG_ERROR, n, "Password mismatch");
	return TRUE;
}

/* List of responses that should be sent to all clients */
static const int response_all[] = { RPL_NOWAWAY, RPL_UNAWAY, 
	ERR_NO_OP_SPLIT, RPL_HIDINGHOST,
	ERR_NEEDREGGEDNICK, RPL_UMODEIS, RPL_SNOMASK,
	RPL_LUSERCLIENT, RPL_LUSEROP, RPL_LUSERUNKNOWN, RPL_LUSERCHANNELS,
	RPL_LUSERME, ERR_NO_OP_SPLIT, RPL_LOCALUSERS, RPL_GLOBALUSERS, 
	RPL_NAMREPLY, RPL_ENDOFNAMES, RPL_TOPIC, RPL_TOPICWHOTIME, 
	RPL_CHANNEL_HOMEPAGE, RPL_CREATIONTIME, RPL_LOGGEDINAS, 0 };
static const int response_none[] = { ERR_NOMOTD, RPL_MOTDSTART, RPL_MOTD, 
	RPL_ENDOFMOTD, 0 };
static const struct {
	int response;
	gboolean (*handler) (struct irc_network *n, const struct irc_line *);
} response_handler[] = {
	{ ERR_PASSWDMISMATCH, handle_464 },
	{ ERR_ALREADYREGISTERED, handle_462 },
	{ ERR_NOPERMFORHOST, handle_463 },
	{ ERR_NOTREGISTERED, handle_451 },
	{ ERR_YOUREBANNEDCREEP, handle_465 },
	{ 0, NULL }
};

/**
 * Redirect a response received from the server.
 *
 * @return TRUE if the message was redirected to zero or more clients, 
 *         FALSE if it was sent to all clients.
 */
gboolean redirect_response(struct query_stack *stack, 
						   struct irc_network *network,
						   const struct irc_line *l)
{
	struct irc_client *c = NULL;
	int n;
	int i;

	c = (struct irc_client *)query_stack_match_response(stack, l);
	if (c != NULL) {
		client_send_line(c, l, NULL);
		return TRUE;
	}
	
	n = irc_line_respcode(l);

	/* See if this is a response that should be sent to all clients */
	for (i = 0; response_all[i]; i++) {
		if (response_all[i] == n) {
			clients_send(network->clients, l, c);
			return FALSE;
		}
	}

	/* See if this is a response that shouldn't be sent to clients at all */
	for (i = 0; response_none[i]; i++) {
		if (response_none[i] == n) {
			return TRUE;
		}
	}

	/* Handle response using custom function */
	for (i = 0; response_handler[i].handler; i++) {
		if (response_handler[i].response == n) {
			return response_handler[i].handler(network, l);
		}
	}

	if (!c) {
		network_log((g_list_length(network->clients) <= 1)?LOG_TRACE:LOG_WARNING, 
					network, "Unable to redirect response %s", l->args[0]);
		clients_send(network->clients, l, NULL);
	}

	return FALSE;
}
