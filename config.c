/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2004 Jelmer Vernooij <jelmer@nl.linux.org>

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

#include "internals.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

xmlNodePtr xmlNode_networks = NULL, xmlNode_plugins = NULL;
xmlDocPtr configuration;
char *configuration_file;

void save_configuration()
{
	xmlSaveFile(configuration_file, configuration);
}

void readConfig(char *file) {
    xmlNodePtr cur;

	configuration = xmlParseFile(file);
	if(!configuration) {
		g_error(_("Can't open configuration file '%s'"), file);
		exit(1);
	}

	cur = xmlDocGetRootElement(configuration);
	g_assert(cur);

	g_assert(!strcmp(cur->name, "ctrlproxy"));

	cur = cur->xmlChildrenNode;
	while(cur) {
		if(xmlIsBlankNode(cur) || !strcmp(cur->name, "comment")) {
			cur = cur->next;
			continue;
		}
		
		if(!strcmp(cur->name, "plugins")) {
			xmlNode_plugins = cur;
		} else if(!strcmp(cur->name, "networks")) {
			xmlNode_networks = cur;
		} else {
			g_warning("Encountered unknown element : %s\n", cur->name);
		}
		
		cur = cur->next;
	}
}

xmlNodePtr config_node_networks() { return xmlNode_networks; }
xmlNodePtr config_node_plugins() { return xmlNode_plugins; }
xmlNodePtr config_node_root() { 
	if(configuration) return configuration->children; 
	return NULL;
}
xmlDocPtr config_doc() { return configuration; }
