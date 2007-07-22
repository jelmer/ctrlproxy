/* 
	ctrlproxy: A modular IRC proxy
	(c) 2003-2007 Jelmer Vernooij <jelmer@nl.linux.org>

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
	GIOChannel *fd;
	GError *error = NULL;

	if (!c->network->global->config->motd_file)
		return NULL;

	if (!strcmp(c->network->global->config->motd_file, ""))
		return NULL;

	fd = g_io_channel_new_file(c->network->global->config->motd_file, "r", &error);
	if(!fd) {
		log_global(LOG_ERROR, "Can't open '%s'", c->network->global->config->motd_file);
		return NULL;
	}

	while (1) {
		GIOStatus status;
		char *buf;
		gsize eol;

		status = g_io_channel_read_line(fd, &buf, NULL, &eol, &error);
		if (status == G_IO_STATUS_EOF) break;
		if (buf[eol] == '\n' || buf[eol] == '\r') buf[eol] = '\0';
		lines = g_realloc(lines, (nrlines+2) * sizeof(char *));
		lines[nrlines] = buf;
		nrlines++;
		lines[nrlines] = NULL;
	}

	g_io_channel_close(fd);

	return lines;
}
