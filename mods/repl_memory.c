/* 
	ctrlproxy: A modular IRC proxy
	(c) 2002-2003 Jelmer Vernooij <jelmer@nl.linux.org>

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
#include "ctrlproxy.h"
#include <string.h>

gboolean fini_plugin(struct plugin *p) {
	return TRUE;
}

static xmlNodePtr new_plugin_node(char *name) {
	xmlNodePtr new;
	new = xmlNewNode(NULL, "plugin");
	xmlSetProp(new, "autoload", "1");
	xmlSetProp(new, "file", name);
	return new;
}

gboolean init_plugin(struct plugin *p) {
	xmlNodePtr xml_memory = new_plugin_node("linestack_memory"), xml_simple = new_plugin_node("repl_simple");
	/* Load linestack_memory and repl_simple */
	return load_plugin(xml_memory) && load_plugin(xml_simple);
}
