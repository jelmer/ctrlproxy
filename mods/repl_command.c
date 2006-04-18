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
#include <string.h>
#include "admin.h"

static GHashTable *markers = NULL;

static void repl_command(struct client *c, char **args, void *userdata)
{
	struct linestack_marker *lm = g_hash_table_lookup(markers, c->network);

	if(!args[1]) {
		admin_out(c, "Sending backlog for network '%s'", c->network->name);

		linestack_send(c->network->global->linestack, c->network, lm, NULL, c);

		g_hash_table_replace(markers, c->network, linestack_get_marker(c->network->global->linestack, c->network));

		return;
	} 

	/* Backlog for specific nick/channel */
	admin_out(c, "Sending backlog for channel %s", args[1]);

	if (c->network->global->config->report_time)
		linestack_send_object_timed(c->network->global->linestack, c->network, args[1], lm, NULL, c);
	else
		linestack_send_object(c->network->global->linestack, c->network, args[1], lm, NULL, c);

	g_hash_table_replace(markers, c->network, linestack_get_marker(c->network->global->linestack, c->network));
}

static const struct admin_command cmd_backlog = {
	"BACKLOG",
	repl_command, 
	"[channel]",
	"Send backlogs for this network or a channel, if specified"
};

static void fini_plugin(void) 
{
	g_hash_table_destroy(markers);
}

static gboolean init_plugin(void)
{
	register_admin_command(&cmd_backlog);
	markers = g_hash_table_new_full(NULL, NULL, NULL, (GDestroyNotify)linestack_free_marker);
	atexit(fini_plugin);
	return TRUE;
}

struct plugin_ops plugin = {
	.name = "repl_command",
	.version = 0,
	.init = init_plugin,
};
