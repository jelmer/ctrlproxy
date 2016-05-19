/*
	ctrlproxy: A modular IRC proxy
	(c) 2003-2008 Jelmer Vernooij <jelmer@jelmer.uk>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
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
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <unistd.h>
#include "keyfile.h"
#include "irc.h"

gboolean keyfile_read_file(const char *filename, char commentchar, GList **nicks)
{
    GIOChannel *gio;
    char *ret;
    gsize nr, term;
	gsize lineno = 0;

    gio = g_io_channel_new_file(filename, "r", NULL);

    if (gio == NULL) {
        return FALSE;
    }

    while (G_IO_STATUS_NORMAL == g_io_channel_read_line(gio, &ret, &nr, &term, NULL))
    {
        char **parts;
		struct keyfile_entry *e;
		lineno++;

		if (ret[0] == commentchar) {
        	g_free(ret);
			continue;
		}

		ret[term] = '\0';

        parts = g_strsplit(ret, "\t", 3);
        g_free(ret);

		if (!parts[0] || !parts[1]) {
			log_global(LOG_WARNING, "%s:%ld: Invalid syntax", filename, (long)lineno);
			g_strfreev(parts);
			continue;
		}
			
		e = g_new0(struct keyfile_entry, 1);
		e->nick = parts[0];
		e->pass = parts[1];
		if (!parts[2] || !strcmp(parts[2], "*")) {
			e->network = NULL;
			g_free(parts[2]);
		} else {
			e->network = parts[2];
		}
	
		*nicks = g_list_append(*nicks, e);
        g_free(parts);
    }

	g_io_channel_shutdown(gio, TRUE, NULL);
	g_io_channel_unref(gio);

	return TRUE;
}

gboolean keyfile_write_file(GList *nicks, const char *header,
							const char *filename)
{
	GList *gl;
	int fd;
	gboolean empty = TRUE;

	fd = open(filename, O_WRONLY | O_CREAT, 0600);

    if (fd == -1) {
		log_global(LOG_WARNING, "Unable to write nickserv file `%s': %s", filename, strerror(errno));
        return FALSE;
    }

	if (write(fd, header, strlen(header)) < 0) {
		log_global(LOG_WARNING, "error writing file header: %s", strerror(errno));
		close(fd);
		return FALSE;
	}

	for (gl = nicks; gl; gl = gl->next) {
		struct keyfile_entry *n = gl->data;
        char *line;

		empty = FALSE;
 
        line = g_strdup_printf("%s\t%s\t%s\n", n->nick, n->pass, n->network?n->network:"*");
		if (write(fd, line, strlen(line)) < 0) {
			log_global(LOG_WARNING, "error writing line `%s': %s", line, strerror(errno));
			g_free(line);
			close(fd);
			return FALSE;
		}

        g_free(line);
	}

    close(fd);

	return TRUE;
}


