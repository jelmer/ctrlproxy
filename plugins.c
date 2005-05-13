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
	if (!p->loaded) 
		return FALSE;

	/* Run exit function if present */
	if(!p->ops->fini(p)) {
		log_global(NULL, "Unable to unload plugin '%s': still in use?", p->ops->name);
		return FALSE;
	}

#ifndef VALGRIND
	g_module_close(p->module);
#endif

	p->loaded = FALSE;

	return TRUE;
}

struct plugin *new_plugin(const char *name)
{
	struct plugin *p = g_new0(struct plugin, 1);

	p->path = g_strdup(name);

	plugins = g_list_append(plugins, p);

	return p;
}

gboolean plugin_loaded(const char *name)
{
	GList *gl = plugins;
	while(gl) {
		struct plugin *p = (struct plugin *)gl->data;
		if(p && p->ops && p->ops->name && !strcmp(p->ops->name, name)) return TRUE;
		gl = gl->next;
	}
	return FALSE;
}

gboolean load_plugin(struct plugin *p)
{
	GModule *m;
	const char *modulesdir;
	struct plugin_ops *ops = NULL;
	extern gboolean plugin_load_config(struct plugin *);
	gchar *path_name;

	/* Determine correct modules directory */
	if(getenv("MODULESDIR"))modulesdir = getenv("MODULESDIR");
	else modulesdir = get_modules_path();

	if(g_file_test(p->path, G_FILE_TEST_EXISTS))path_name = g_strdup(p->path);
	else path_name = g_module_build_path(modulesdir, p->path);
	
	m = g_module_open(path_name, G_MODULE_BIND_LAZY);

	if(!m) {
		log_global(NULL, "Unable to open module %s(%s), ignoring", path_name, g_module_error());
		g_free(path_name);
		return FALSE;
	}

	if(!g_module_symbol(m, "plugin", (gpointer)&ops)) {
		log_global(strchr(path_name, '/')?(strrchr(path_name, '/')+1):NULL, 
				   "No valid plugin information found");
		g_free(path_name);
		return FALSE;
	}

	if(plugin_loaded(ops->name)) {
		log_global(NULL, "Plugin already loaded");
		g_free(path_name);
		return FALSE;
	}

	g_free(path_name);

	p->module = m;
	p->ops = ops;

	if(!p->ops->init(p)) {
		log_global(NULL, "Running initialization function for plugin '%s' failed.", p->path);
		return FALSE;
	}

	if(!plugin_load_config(p)) {
		log_global(NULL, "Error loading configuration for plugin '%s'", p->path);
	}

	p->loaded = TRUE;
	return TRUE;
}

void free_plugin(struct plugin *p)
{
	g_free(p->path);
	g_free(p);

	plugins = g_list_remove(plugins, p);
}

void fini_plugins() {
	while (plugins) 
	{
		struct plugin *p = plugins->data;

		unload_plugin(p);

		free_plugin(p);
	}
}

gboolean init_plugins() {
	gboolean ret = TRUE;

	if(!g_module_supported()) {
		log_global(NULL, "DSO's not supported on this platform. Not loading any modules");
	} else if(!plugins) {
		log_global(NULL, "No modules set to be loaded");	
	} else {
		GList *gl = plugins;
		while(gl) {
			struct plugin *p = gl->data;

			if(!p->loaded && p->autoload && !load_plugin(p)) {
				log_global(NULL, "Can't load plugin %s, ignoring...", p->ops?p->ops->name:p->path);
				ret = FALSE;
			}

			gl = gl->next;
		}
	}

	return ret;
}

GList *get_plugin_list() {
	return plugins;
}
