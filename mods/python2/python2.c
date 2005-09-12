/* 
	ctrlproxy: A modular IRC proxy
	(c) 2005 Jelmer Vernooij <jelmer@nl.linux.org>

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

#include <Python.h>
#include "ctrlproxy.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <glib.h>

static gboolean fini_plugin(struct plugin *p)
{
	Py_Finalize();
	return TRUE;
}

static gboolean load_config(struct plugin *p, xmlNodePtr node)
{
	FILE *fd;
	xmlNodePtr cur;

	Py_Initialize();
	
	for (cur = node->xmlChildrenNode; cur; cur = cur->next)
	{
		if(!xmlIsBlankNode(cur) && !strcmp(cur->name, "script")) {
			const char *filename = xmlNodeGetContent(cur);
			fd = fopen(filename, "r");
			if (!fd) {
				log_global("python", LOG_ERROR, "Unable to open python script %s: %s", filename, strerror(errno));
			} else {
				PyRun_SimpleFile(fd, filename);
				fclose(fd);
			}
		}
	}

	return TRUE;
}

static gboolean init_plugin(struct plugin *p)
{
	return TRUE;
}

struct plugin_ops plugin = {
	.name = "python2",
	.version = 0,
	.init = init_plugin,
	.fini = fini_plugin,
	.load_config = load_config
};
