/* 
	ctrlproxy: A modular IRC proxy
	(c) 2003 Jelmer Vernooij <jelmer@nl.linux.org>

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

#define _GNU_SOURCE
#include <time.h>
#include "ctrlproxy.h"
#include "../config.h"
#include "admin.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "ctcp"

static void dcc_command(char **args, struct line *l)
{
	if(!strcasecmp(args[1], "GET")) {
		/* FIXME */
	} else if(!strcasecmp(args[1], "LIST")) {
		/* FIXME */
	} else if(!strcasecmp(args[1], "DEL")) {
		/* FIXME */
	} else {
		admin_out(l, "Unknown subcommand '%s'", args[1]);
	}
}

static gboolean mhandle_data(struct line *l)
{
	if(l->direction == TO_SERVER || l->args[1][0] != '\001') return TRUE;
	/* FIXME */
	return TRUE;
}

gboolean fini_plugin(struct plugin *p)
{
	del_filter(mhandle_data);
	return TRUE;
}

gboolean init_plugin(struct plugin *p) 
{
	if(!plugin_loaded("admin") && !plugin_loaded("libadmin")) {
		g_warning("admin module required for dcc module. Please load it first");
		return FALSE;
	}
	register_admin_command("DCC", dcc_command);
	add_filter("dcc", mhandle_data);
	return TRUE;
}



