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
#include <dirent.h>
#include <errno.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "ctcp"

static void dcc_list(struct line *l)
{
	char *p = ctrlproxy_path("dcc");
	DIR *d = opendir(p);	
	struct dirent *e;
	free(p);

	if(!d) {
		g_warning("Can't open directory '%s'", p);
		admin_out(l, "Can't open directory '%s'", p);
		return;
	}

	while((e = readdir(d))) {
		admin_out(l, "%s", e->d_name);
	}

	closedir(d);
}

static void dcc_del(struct line *l, const char *f)
{
	char *p;
	char *r;
	if(strchr(f, '/')) {
		admin_out(l, "Invalid filename");
		return;
	}

	asprintf(&r, "dcc/%s", f);
	p = ctrlproxy_path(r);
	free(r);

	if(unlink(p) != 0) {
		admin_out(l, "Error occured while deleting %s: %s", p, strerror(errno));
	} else {
		admin_out(l, "%s successfully deleted", p);
	}
	free(p);
}

static void dcc_send(struct line *l, const char *f)
{
	/* FIXME */
}

static void dcc_command(char **args, struct line *l)
{
	if(!strcasecmp(args[2], "GET")) {
		dcc_send(l, args[3]);
	} else if(!strcasecmp(args[2], "LIST")) {
		dcc_list(l);
	} else if(!strcasecmp(args[2], "DEL")) {
		dcc_del(l, args[3]);
	} else {
		admin_out(l, "Unknown subcommand '%s'", args[2]);
	}
}

static gboolean mhandle_data(struct line *l)
{
	if(l->direction == TO_SERVER || l->args[1][0] != '\001') return TRUE;

	/* FIXME: DCC SEND/FILE */

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
	register_admin_command("DCC", dcc_command, NULL, NULL);
	add_filter("dcc", mhandle_data);
	return TRUE;
}
