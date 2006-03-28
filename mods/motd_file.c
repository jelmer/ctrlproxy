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
		log_global("motd_file", LOG_ERROR, "Can't open '%s'", motd_file);
		return NULL;
	}

	while(!feof(fd)) {
		char buf[512]; char *eol;
		if(!fgets(buf, sizeof(buf), fd))break;
		lines = g_realloc(lines, (nrlines+2) * sizeof(char *));
		lines[nrlines] = g_strdup(buf);
		if ((eol = strchr(lines[nrlines], '\n'))) *eol = '\0';
		if ((eol = strchr(lines[nrlines], '\r'))) *eol = '\0';
		nrlines++;
		lines[nrlines] = NULL;
	}

	fclose(fd);

	return lines;
}

static gboolean fini_plugin(struct plugin *p) {
	
	del_motd_hook("motd_file");
	g_free(motd_file);
	motd_file = NULL;
	return TRUE;
}

static gboolean load_config(struct plugin *p, xmlNodePtr node) 
{
	xmlNodePtr cur;
	extern struct global *_global;

	for (cur = node->children; cur; cur = cur->next)
	{
		if (cur->type != XML_ELEMENT_NODE) continue;
		
		if (!strcmp(cur->name, "motd_file")) {
			motd_file = xmlNodeGetContent(cur);
		}
	}

	if (!motd_file) motd_file = g_build_filename(_global->config->shared_path, "motd", NULL);

	if(!g_file_test(motd_file, G_FILE_TEST_EXISTS)) {
		log_global("motd_file", LOG_ERROR, "Can't open MOTD file '%s' for reading", motd_file);
		return FALSE;
	}

	return TRUE;
}

static gboolean init_plugin(struct plugin *p) {
	add_motd_hook("motd_file", motd_file_handler, NULL);
	return TRUE;
}

struct plugin_ops plugin = {
	.name = "motd_file",
	.version = 0,
	.init = init_plugin,
	.fini = fini_plugin,
	.load_config = load_config,
};
