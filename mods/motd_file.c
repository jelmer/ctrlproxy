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

#include "ctrlproxy.h"
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include "irc.h"

static char *motd_file = NULL;

char ** motd_file_handler(struct network *n, void *userdata)
{
	char **lines = NULL;
	size_t nrlines = 0;
	FILE *fd;

	fd = fopen(motd_file, "r");
	if(!fd) {
		g_warning(("Can't open '%s'\n"), motd_file);
		return NULL;
	}

	while(!feof(fd)) {
		char buf[512];
		if(!fgets(buf, sizeof(buf), fd))break;
		lines = g_realloc(lines, (nrlines+2) * sizeof(char *));
		lines[nrlines] = g_strdup(buf);
		nrlines++;
		lines[nrlines] = NULL;
	}

	fclose(fd);

	return lines;
}

gboolean fini_plugin(struct plugin *p) {
	
	del_motd_hook("motd_file");
	g_free(motd_file);
	motd_file = NULL;
	return TRUE;
}

const char name_plugin[] = "motd_file";

gboolean load_config(struct plugin *p, xmlNodePtr node) 
{
	xmlNodePtr cur;

	for (cur = node->children; cur; cur = cur->next)
	{
		if (cur->type != XML_ELEMENT_NODE) continue;
		
		if (!strcmp(cur->name, "motd_file")) {
			motd_file = xmlNodeGetContent(cur);
		}
	}

	if (!motd_file) motd_file = g_build_filename(get_shared_path(), "motd", NULL);

	if(!g_file_test(motd_file, G_FILE_TEST_EXISTS)) {
		g_warning(("Can't open MOTD file '%s' for reading\n"), motd_file);
		return FALSE;
	}

	return TRUE;
}

gboolean init_plugin(struct plugin *p) {
	add_motd_hook("motd_file", motd_file_handler, NULL);
	return TRUE;
}
