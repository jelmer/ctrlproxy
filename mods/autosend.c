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

#include "ctrlproxy.h"
#include <string.h>

struct autosend_line 
{
	struct line *line;
	char *network;
};

static GList *autosend_lines = NULL;

static void do_autosend (struct network *n, void *ud)
{
	GList *gl;
	for (gl = autosend_lines; gl; gl = gl->next) {
		struct autosend_line *al = gl->data;
		if (!al->network || !strcmp(al->network, n->name)) 
			network_send_line(n, NULL, al->line);
	}
}

static gboolean fini_plugin(struct plugin *p) 
{
	del_server_connected_hook("autosend");
	return TRUE;
}

static gboolean save_config(struct plugin *p, xmlNodePtr node)
{
	GList *gl;

	for (gl = autosend_lines; gl; gl = gl->next) {
		struct autosend_line *al = gl->data;
		xmlNodePtr n;
		char *tmp = irc_line_string(al->line);
		
		n = xmlNewTextChild(node, NULL, "autosend", tmp);

		if (al->network)
			xmlSetProp(n, "network", al->network);

		g_free(tmp);
	}

	return TRUE;
}

static gboolean load_config(struct plugin *p, xmlNodePtr node)
{
	struct autosend_line *al;
	
	xmlNodePtr n;
	for (n = node->xmlChildrenNode; n; n = n->next) {
		xmlChar *tmp;
		al = g_new0(struct autosend_line, 1);
		al->network = xmlGetProp(n, "network");
		tmp = xmlNodeGetContent(n);
		al->line = irc_parse_line(tmp);
		xmlFree(tmp);
		autosend_lines = g_list_append(autosend_lines, al);
	}

	return TRUE;
}

static gboolean init_plugin(struct plugin *p) 
{
	add_server_connected_hook("autosend", do_autosend, NULL);
	return TRUE;
}

struct plugin_ops plugin = {
	.name = "autosend",
	.version = 0,
	.init = init_plugin,
	.fini = fini_plugin,
	.load_config = load_config,
	.save_config = save_config
};
