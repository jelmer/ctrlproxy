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

static void do_crash(char **args, struct line *l, void *userdata)
{
	char *foo = NULL;
	*foo='a';
}

static void dump_joined_channels(char **args, struct line *l, void *userdata)
{
	struct network *n = l->network;
	GList *gl;

	if(args[1]) {
		n = find_network(args[1]);
		if(!n) {
			admin_out(l, ("Can't find network '%s'"), args[1]);
			return;
		}
	}

	gl = n->channels;
	while(gl) {
		struct channel *c = (struct channel *)gl->data;
		admin_out(l, "%s", c->name);
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
	"CRASH", do_crash, "", NULL, NULL
};

gboolean fini_plugin(struct plugin *p) {
	unregister_admin_command(&cmd_dumpjoined);
	unregister_admin_command(&cmd_crash);
	return TRUE;
}

const char name_plugin[] = "debug";

gboolean init_plugin(struct plugin *p) {
	if(!plugin_loaded("admin")) {
		g_warning(("admin module required for repl_command module. Please load it first"));
		return FALSE;
	}
	register_admin_command(&cmd_dumpjoined);
	register_admin_command(&cmd_crash);
	return TRUE;
}
