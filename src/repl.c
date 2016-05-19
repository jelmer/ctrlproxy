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
#include "irc.h"
#include "repl.h"

static GList *backends = NULL;

void register_replication_backend(const struct replication_backend *backend)
{
	backends = g_list_append(backends, g_memdup(backend, sizeof(*backend)));
}

struct replication_backend *repl_find_backend(const char *name)
{
	GList *gl;

	if (name == NULL)
		name = "none";

	for (gl = backends; gl; gl = gl->next) {
		struct replication_backend *backend = gl->data;
		if (!strcmp(backend->name, name))
			return backend;
	}

	return NULL;
}

/**
 * Replicate the current state and backlog to the client.
 *
 * @param client Client to send data to.
 */
void client_replicate(struct irc_client *client)
{
	const char *bn = client->network->global->config->replication;
	struct replication_backend *backend;
	
	backend = repl_find_backend(bn);

	if (backend == NULL) {
		client_log(LOG_WARNING, client, 
				   "Unable to find replication backend '%s'", bn);

		if (client->network->external_state)
			client_send_state(client, client->network->external_state);

		return;
	} 

	if (client->network->linestack == NULL) {
		if (client->network->external_state)
			client_send_state(client, client->network->external_state);

		return;
	}

	backend->replication_fn(client);
}
