/*
	ctrlproxy: A modular IRC proxy
	help: module for access the help system

	(c) 2006 Jelmer Vernooij <jelmer@nl.linux.org>

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

#include "internals.h"

void admin_cmd_help(admin_handle h, char **args, void *userdata)
{
	extern GList *admin_commands;
	extern guint longest_command;
	GList *gl = admin_commands;
	char *tmp;
	char **details;
	int i;

	if(args[1]) {
		admin_out(h, "Details for command %s:", args[1]);
	} else {
		admin_out(h, "The following commands are available:");
	}
	while(gl) {
		struct admin_command *cmd = (struct admin_command *)gl->data;
		if(args[1]) {
			if(!g_strcasecmp(args[1], cmd->name)) {
				if(cmd->help_details != NULL) {
					details = g_strsplit(cmd->help_details, "\n", 0);
					for(i = 0; details[i] != NULL; i++) {
						admin_out(h, details[i]);
					}
					return;
				} else {
					admin_out(h, "Sorry, no help for %s available", args[1]);
				}
			}
		} else {
			if(cmd->help != NULL) {
				tmp = g_strdup_printf("%s%s     %s",cmd->name,g_strnfill(longest_command - strlen(cmd->name),' '),cmd->help);
				admin_out(h, tmp);
				g_free(tmp);
			} else {
				admin_out(h, cmd->name);
			}
		}
		gl = gl->next;
	}
	if(args[1]) {
		admin_out(h, "Unknown command");
	}
}
