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

/* globals */
struct plugin *current_plugin = NULL;
GList *plugins = NULL;

gboolean unload_plugin(struct plugin *p)
{
	plugin_fini_function f;

	/* Run exit function if present */
	if(g_module_symbol(p->module, "fini_plugin", (gpointer)&f)) {
		if(!f(p)) {
			g_warning(_("Unable to unload plugin '%s': still in use?"), p->name);
			return FALSE;
		}
	} else {
		g_warning(_("No symbol 'fini_plugin' in plugin '%s'. Module does not support unloading, so no unload will be attempted"), p->name);
		return FALSE;
	}

	g_module_close(p->module);

	/* Remove autoload from this plugins' element */
	xmlSetProp(p->xmlConf, "autoload", "0");

	plugins = g_list_remove(plugins, p);
	g_free(p->name);
	g_free(p->path);
	g_free(p);
	return TRUE;
}

gboolean plugin_loaded(char *name)
{
	GList *gl = plugins;
	while(gl) {
		struct plugin *p = (struct plugin *)gl->data;
		if(!strcmp(p->name, name)) return TRUE;
		gl = gl->next;
	}
	return FALSE;
}

gboolean load_plugin(xmlNodePtr cur)
{
	GModule *m;
	char *mod_name;
	struct plugin *p;
	const char *modulesdir;
	char *selfname = NULL;
	gchar *path_name;
	plugin_init_function f = NULL;

	mod_name = xmlGetProp(cur, "file");
	if(!mod_name) {
		g_warning(_("No filename specified for plugin"));
		return FALSE;
	}

	/* Determine correct modules directory */
	if(getenv("MODULESDIR"))modulesdir = getenv("MODULESDIR");
	else modulesdir = get_modules_path();

	if(g_file_test(mod_name, G_FILE_TEST_EXISTS))path_name = g_strdup(mod_name);
	else path_name = g_module_build_path(modulesdir, mod_name);
	
	m = g_module_open(path_name, G_MODULE_BIND_LAZY);

	if(!m) {
		g_warning(_("Unable to open module %s(%s), ignoring"), path_name, g_module_error());
		return FALSE;
	}
	g_free(path_name);

	if(!g_module_symbol(m, "name_plugin", (gpointer)&selfname)) {
		selfname = mod_name;
	}

	if(plugin_loaded(selfname)) {
		g_warning(_("Plugin already loaded"));
		xmlFree(mod_name);
		return FALSE;
	}

	if(!g_module_symbol(m, "init_plugin", (gpointer)&f)) {
		g_warning(_("Can't find symbol 'init_plugin' in module %s"), path_name);
		xmlFree(mod_name);
		return FALSE;
	}
	
	p = g_new(struct plugin, 1);
	p->name = g_strdup(selfname);
	p->path = g_strdup(mod_name);
	p->module = m;
	p->xmlConf = cur;

	push_plugin(p);
	if(!f(p)) {
		g_warning(_("Running initialization function for plugin '%s' failed."), mod_name);
		g_free(p->name);
		g_free(p->path);
		xmlFree(mod_name);
		g_free(p);
		return FALSE;
	}
	pop_plugin();

	plugins = g_list_append(plugins, p);
	
	xmlSetProp(cur, "autoload", "1");
	xmlFree(mod_name);
	return TRUE;
}

gboolean init_plugins() {
	gboolean ret = TRUE;
	xmlNodePtr cur;
	if(!g_module_supported()) {
		g_warning(_("DSO's not supported on this platform. Not loading any modules"));
	} else if(!config_node_plugins()) {
		g_warning(_("No modules set to be loaded"));	
	}else {
		cur = config_node_plugins()->xmlChildrenNode;
		while(cur) {
			char *enabled;

			if(xmlIsBlankNode(cur) || !strcmp(cur->name, "comment")){ cur = cur->next; continue; }

			g_assert(!strcmp(cur->name, "plugin"));

			enabled = xmlGetProp(cur, "autoload");
			if((!enabled || atoi(enabled) == 1) && !load_plugin(cur)) {
				g_error(_("Can't load plugin %s, aborting..."), xmlGetProp(cur, "file"));
				ret = FALSE;
			}

			xmlFree(enabled);

			cur = cur->next;
		}
	}
	return ret;
}

GList *get_plugin_list() {
	return plugins;
}

void push_plugin(struct plugin *p) {
	/*FIXME*/
}

struct plugin * pop_plugin() {
	/*FIXME*/
	return NULL;
}

struct plugin *peek_plugin() { /*FIXME*/ return NULL; }

