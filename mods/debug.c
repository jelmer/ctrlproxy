/*
	ctrlproxy: A modular IRC proxy
	(c) 2004 Jelmer Vernooij <jelmer@nl.linux.org>

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

static void do_abort(const struct client *c, char **args, void *userdata)
{
	abort();
}

static void dump_joined_channels(const struct client *c, char **args, void *userdata)
{
	struct network *n = c->network;
	GList *gl;

	if(args[1]) {
		n = find_network(args[1]);
		if(!n) {
			admin_out(c, "Can't find network '%s'", args[1]);
			return;
		}
	}

	gl = n->state.channels;
	while(gl) {
		struct channel_state *ch = (struct channel_state *)gl->data;
		admin_out(c, "%s", ch->name);
		gl = gl->next;
	}
}

static const struct admin_command cmd_dumpjoined = {
	"DUMPJOINEDCHANNELS",
	dump_joined_channels, 
	"[network]", 
	NULL, 
	NULL
};

static const struct admin_command cmd_crash = {
	"ABORT", do_abort, "", NULL, NULL
};

static gboolean fini_plugin(struct plugin *p) {
	unregister_admin_command(&cmd_dumpjoined);
	unregister_admin_command(&cmd_crash);
	return TRUE;
}

static gboolean init_plugin(struct plugin *p) {
	if(!plugin_loaded("admin")) {
		log_global("debug", "admin module required for repl_command module. Please load it first");
		return FALSE;
	}
	register_admin_command(&cmd_dumpjoined);
	register_admin_command(&cmd_crash);
	return TRUE;
}

struct plugin_ops plugin = {
	.name = "debug",
	.version = 0,
	.init = init_plugin,
	.fini = fini_plugin,
};
