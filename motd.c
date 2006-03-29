/* 
	ctrlproxy: A modular IRC proxy
	(c) 2003,2006 Jelmer Vernooij <jelmer@nl.linux.org>

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

char ** get_motd_lines(struct client *c)
{
	char **lines = NULL;
	size_t nrlines = 0;
	FILE *fd;

	if (!c->network->global->config->motd_file)
		return NULL;

	if (!strcmp(c->network->global->config->motd_file, ""))
		return NULL;

	fd = fopen(c->network->global->config->motd_file, "r");
	if(!fd) {
		log_global(NULL, LOG_ERROR, "Can't open '%s'", c->network->global->config->motd_file);
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
