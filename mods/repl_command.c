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
#include "gettext.h"
#define _(s) gettext(s)

static GHashTable *command_backlog = NULL;

static gboolean log_data(struct line *l, void *userdata) {
	struct linestack_context *co;
	char *desc;

	if(l->argc < 1)return TRUE;

	if(g_strcasecmp(l->args[0], "PRIVMSG") && g_strcasecmp(l->args[0], "NOTICE"))return TRUE;

	/* Lookup this channel */
	desc = g_strdup_printf("%s/%s", l->network->name, l->args[1]);
	
	co = g_hash_table_lookup(command_backlog, desc);
	
	if(!co) {
		co = linestack_new_by_network(l->network);
		g_hash_table_insert(command_backlog, desc, co);
	} else g_free(desc);
	linestack_add_line(co, l);

	return TRUE;
}

gboolean fini_plugin(struct plugin *p) {
	del_filter_ex("replicate", log_data);
	unregister_admin_command("BACKLOG");
	g_hash_table_destroy(command_backlog); command_backlog = NULL;
	return TRUE;
}

static void replicate_channel(gpointer key, gpointer val, gpointer user)
{
	struct linestack_context *co = (struct linestack_context *)val;
	struct line *l = (struct line *)user;

	if(g_ascii_strncasecmp(l->network->name, key, strlen(l->network->name)))
		return;

	linestack_send(co, l->client->incoming);
	linestack_clear(co);
}

static void repl_command(char **args, struct line *l, void *userdata)
{
	struct linestack_context *co;
	char *desc;

	if(!command_backlog) { 
		admin_out(l, _("No backlogs saved yet"));
		return;
	}
	
	if(!args[1]) {
		admin_out(l, _("Sending backlog for network '%s'"), l->network->name);

		/* Backlog everything for this network */
		g_hash_table_foreach(command_backlog, replicate_channel, l);
		return;
	} 

	/* Backlog for specific nick/channel */
	admin_out(l, _("Sending backlog for channel %s@%s"), args[1], l->network->name);
	desc = g_strdup_printf("%s/%s", l->network->name, args[1]);
	co = g_hash_table_lookup(command_backlog, desc);
	g_free(desc);

	if(co)  {
		linestack_send(co, l->client->incoming);
		linestack_clear(co);
	} else {
		admin_out(l, _("No backlog for %s"), args[1]);
	}
}

const char name_plugin[] = "repl_command";

gboolean init_plugin(struct plugin *p) {
	if(!plugin_loaded("admin")) {
		g_warning(_("admin module required for repl_command module. Please load it first"));
		return FALSE;
	}
	add_filter_ex("repl_command", log_data, NULL, "replicate", 1000);
	register_admin_command("BACKLOG", repl_command, _("[channel]"), _("Send backlogs for this network or a channel, if specified"), NULL);
	command_backlog = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, linestack_destroy);
	return TRUE;
}
