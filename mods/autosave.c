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

static int interval_time = 0;
static int autosave_id;
static struct plugin *this_plugin = NULL;

static gboolean loop_save_config(gpointer user_data)
{
	save_configuration(get_current_config(), NULL);
	return TRUE;
}

static gboolean save_config(struct plugin *p, xmlNodePtr node)
{
	char *tmp;
	xmlNodePtr n = xmlNewTextChild(node, NULL, "interval", NULL);
	tmp = g_strdup_printf("%d", interval_time);
	xmlSetProp(n, "time", tmp);
	g_free(tmp);
	return TRUE;
}

static gboolean load_config(struct plugin *p, xmlNodePtr node)
{
	xmlNodePtr cur;

	for (cur = node->children; cur; cur = cur->next) {
		if(!strcmp(cur->name, "interval")) {
			char *buf = xmlGetProp(cur, "time");
			if(buf)
				interval_time = atoi(buf);
			xmlFree(buf);
		}
	}

	if(interval_time > 0)
		autosave_id = g_timeout_add(1000 * 60 * interval_time, loop_save_config, NULL);
	else
		log_global("autosave", "Interval of %i minutes is too short", interval_time);

	return TRUE;
}

static gboolean init_plugin(struct plugin *p)
{
	this_plugin = p;
	
	return TRUE;
}

static gboolean fini_plugin(struct plugin *p)
{
	return g_source_remove(autosave_id);
}

struct plugin_ops plugin = {
	.name = "autosave",
	.version = 0,
	.init = init_plugin,
	.fini = fini_plugin,
	.load_config = load_config,
	.save_config = save_config
};
