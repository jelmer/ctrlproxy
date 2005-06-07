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

STATIC_MODULE_DECLARES
static struct plugin *builtin_modules[] = { STATIC_MODULES NULL };

gboolean unload_plugin(struct plugin *p)
{
	/* Run exit function if present */
	if(!p->ops->fini(p)) {
		log_global(NULL, "Unable to unload plugin '%s': still in use?", p->ops->name);
		return FALSE;
	}

#ifndef VALGRIND
	g_module_close(p->module);
#endif

	plugins = g_list_remove(plugins, p);

	g_free(p);

	return TRUE;
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

struct plugin *load_plugin(struct plugin_config *pc)
{
	GModule *m;
	const char *modulesdir;
	struct plugin_ops *ops = NULL;
	struct plugin *p = g_new0(struct plugin, 1);
	extern gboolean plugin_load_config(struct plugin *);
	gchar *path_name;

	p->config = pc;

	/* Determine correct modules directory */
	if(getenv("MODULESDIR"))modulesdir = getenv("MODULESDIR");
	else modulesdir = get_current_config()->modules_path;

	if(g_file_test(pc->path, G_FILE_TEST_EXISTS))path_name = g_strdup(pc->path);
	else path_name = g_module_build_path(modulesdir, pc->path);
	
	m = g_module_open(path_name, G_MODULE_BIND_LAZY);

	if(!m) {
		log_global(NULL, "Unable to open module %s(%s), ignoring", path_name, g_module_error());
		g_free(path_name);
		g_free(p);
		return NULL;
	}

	if(!g_module_symbol(m, "plugin", (gpointer)&ops)) {
		log_global(strchr(path_name, '/')?(strrchr(path_name, '/')+1):NULL, 
				   "No valid plugin information found");
		g_free(path_name);
		g_free(p);
		return NULL;
	}

	if(plugin_loaded(ops->name)) {
		log_global(NULL, "Plugin already loaded");
		g_free(path_name);
		g_free(p);
		return NULL;
	}

	g_free(path_name);

	p->module = m;
	p->ops = ops;

	if(!p->ops->init(p)) {
		log_global(NULL, "Running initialization function for plugin '%s' failed.", pc->path);
		g_free(p);
		return NULL;
	}

	if(!plugin_load_config(p)) {
		log_global(NULL, "Error loading configuration for plugin '%s'", pc->path);
	}

	plugins = g_list_append(plugins, p);

	return p;
}

void fini_plugins() {
	while (plugins) 
	{
		struct plugin *p = plugins->data;

		unload_plugin(p);
	}
}

gboolean init_plugins(struct ctrlproxy_config *cfg)
{
	gboolean ret = TRUE;

	if(!g_module_supported()) {
		log_global(NULL, "DSO's not supported on this platform. Not loading any modules");
	} else if(!cfg->plugins) {
		log_global(NULL, "No modules set to be loaded");	
	} else {
		GList *gl = cfg->plugins;
		while(gl) {
			struct plugin_config *p = gl->data;

			if(p->autoload && !load_plugin(p)) {
				log_global(NULL, "Can't load plugin %s, ignoring...", p->path);
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
