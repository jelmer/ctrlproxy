/*
	ctrlproxy: A modular IRC proxy
	autosave: save configuration at a specific interval
	(c) 2004 Daniel Poelzleithner <ctrlproxy@poelzi.org>

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

#include <ctrlproxy.h>
#include <string.h>

static int autosave_id;

static gboolean loop_save_config(gpointer user_data)
{
	save_configuration();
	return TRUE;
}

gboolean init_plugin(struct plugin *p)
{
	int time = 0;
	xmlNodePtr cur = p->xmlConf->children;
	char *buf;
	while(cur) {
		if(!strcmp(cur->name, "interval")) {
			buf = xmlGetProp(cur, "time");
			if(buf)
				time = atoi(buf);
			xmlFree(buf);
		}
		cur = cur->next;
	}
	if(time > 0)
		autosave_id = g_timeout_add(1000 * 60 * time, loop_save_config, NULL);
	else
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, "Interval of %i minutes is too short", time);

	return TRUE;
}

gboolean fini_plugin(struct plugin *p)
{
	return g_source_remove(autosave_id);
}
