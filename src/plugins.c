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

struct plugin *load_plugin(const char *modulesdir, const char *name)
{
	GModule *m = NULL;
	struct plugin_ops *ops = NULL;
	struct plugin *p = g_new0(struct plugin, 1);
	gchar *path_name = NULL;

	/* Try to load from .so file */
	if (!ops) {

		if(g_file_test(name, G_FILE_TEST_EXISTS))path_name = g_strdup(name);
		else path_name = g_module_build_path(modulesdir, name);
	
		m = g_module_open(path_name, 0);

		if(!m) {
			log_global(LOG_ERROR, "Unable to open module %s(%s), ignoring", path_name, g_module_error());
			g_free(path_name);
			g_free(p);
			return NULL;
		}

		if(!g_module_symbol(m, "plugin", (gpointer)&ops)) {
			log_global(LOG_ERROR, "%s: No valid plugin information found", 
				strchr(path_name, '/')?(strrchr(path_name, '/')+1):"error"
					   );
			g_free(path_name);
			g_free(p);
			return NULL;
		}
	}

	if(plugin_loaded(ops->name)) {
		log_global(LOG_WARNING, "%s: Plugin already loaded", ops->name);
		g_free(path_name);
		g_free(p);
		return NULL;
	}

	g_free(path_name);

	p->module = m;
	p->ops = ops;

	if(!p->ops->init()) {
		log_global( LOG_ERROR, "%s: Error during initialization.", p->ops->name);
		g_free(p);
		return NULL;
	}

	plugins = g_list_append(plugins, p);

	return p;
}

gboolean init_plugins(const char *plugin_dir)
{
	gboolean ret = TRUE;

	if(!g_module_supported()) {
		log_global(LOG_WARNING, "DSO's not supported on this platform. Not loading any plugins");
	} else {
		const char *name;
		GDir *dir = g_dir_open(plugin_dir, 0, NULL);
		if (!dir) {
			log_global(LOG_WARNING, "Plugin dir does not exist, not loading plugins");
			return FALSE;
		}

		while ((name = g_dir_read_name(dir))) {
			if (strcmp(name + strlen(name) - strlen(G_MODULE_SUFFIX), G_MODULE_SUFFIX) != 0)
				continue;

			if (!load_plugin(plugin_dir, name))
				ret = FALSE;
		}

		g_dir_close(dir);
	}

	return ret;
}

GList *get_plugin_list() {
	return plugins;
}
