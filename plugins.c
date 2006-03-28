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

struct plugin *plugin_by_config(struct plugin_config *pc)
{
	GList *gl;
	for (gl = plugins; gl; gl = gl->next) {
		struct plugin *p = (struct plugin *)gl->data;
		g_assert(p);
		g_assert(p->ops);
		g_assert(p->ops->name);
		if (!strcmp(p->ops->name, pc->path)) 
			return p;
	}

	return NULL;
}


gboolean unload_plugin(struct plugin *p)
{
	/* Run exit function if present */
	if(!p->ops->fini(p)) {
		log_global(NULL, LOG_ERROR, "Unable to unload plugin '%s': still in use?", p->ops->name);
		return FALSE;
	}

#ifndef VALGRIND
	if (p->module)
		g_module_close(p->module);
#endif

	plugins = g_list_remove(plugins, p);

	g_free(p);

	return TRUE;
}

gboolean plugin_loaded(const char *name)
{
	GList *gl;
	for (gl = plugins; gl; gl = gl->next) {
		struct plugin *p = (struct plugin *)gl->data;
		if(p && p->ops && p->ops->name && !strcmp(p->ops->name, name)) 
			return TRUE;
	}
	return FALSE;
}

struct plugin *load_plugin(const char *dir, struct plugin_config *pc)
{
	GModule *m = NULL;
	const char *modulesdir;
	struct plugin_ops *ops = NULL;
	struct plugin *p = g_new0(struct plugin, 1);
	gchar *path_name = NULL;
	int i;

	g_assert(pc);

	p->config = pc;

	/* Try to load from .so file */
	if (!ops) {
		/* Determine correct modules directory */
		if(getenv("MODULESDIR"))modulesdir = getenv("MODULESDIR");
		else modulesdir = dir;

		if(g_file_test(pc->path, G_FILE_TEST_EXISTS))path_name = g_strdup(pc->path);
		else path_name = g_module_build_path(modulesdir, pc->path);
	
		m = g_module_open(path_name, G_MODULE_BIND_LAZY);

		if(!m) {
			log_global(NULL, LOG_ERROR, "Unable to open module %s(%s), ignoring", path_name, g_module_error());
			g_free(path_name);
			g_free(p);
			return NULL;
		}

		if(!g_module_symbol(m, "plugin", (gpointer)&ops)) {
			log_global(strchr(path_name, '/')?(strrchr(path_name, '/')+1):NULL, 
					   LOG_ERROR, "No valid plugin information found");
			g_free(path_name);
			g_free(p);
			return NULL;
		}
	}

	if(plugin_loaded(ops->name)) {
		log_global(ops->name, LOG_WARNING, "Plugin already loaded");
		g_free(path_name);
		g_free(p);
		return NULL;
	}

	g_free(path_name);

	p->module = m;
	p->ops = ops;

	if(!p->ops->init(p)) {
		log_global(p->ops->name, LOG_ERROR, "Error during initialization.");
		g_free(p);
		return NULL;
	}

	if (p->ops->load_config && p->config->node) {
		if (!p->ops->load_config(p, p->config->node)) {
			log_global(p->ops->name, LOG_ERROR, "Error loading configuration");
			g_free(p);
			return NULL;
		}
	}

	plugins = g_list_append(plugins, p);

	return p;
}

void fini_plugins() 
{
	while (plugins) 
	{
		struct plugin *p = plugins->data;

		unload_plugin(p);
	}
}

gboolean init_plugins(struct ctrlproxy_config *cfg)
{
	gboolean ret = TRUE;

	g_assert(cfg);

	if(!g_module_supported()) {
		log_global(NULL, LOG_WARNING, "DSO's not supported on this platform. Not loading any modules");
	} else if(!cfg->plugins) {
		log_global(NULL, LOG_WARNING, "No modules set to be loaded");	
	} else {
		GList *gl = cfg->plugins;
		while(gl) {
			struct plugin_config *p = gl->data;

			if(p->autoload && !load_plugin(cfg->modules_path, p)) {
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
