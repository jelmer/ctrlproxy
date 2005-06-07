/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2005 Jelmer Vernooij <jelmer@nl.linux.org>

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

#ifndef __CTRLPROXY_PLUGINS_H__
#define __CTRLPROXY_PLUGINS_H__

struct plugin {
	struct plugin_config *config;
	GModule *module;
	void *data;
	struct plugin_ops {
		int version;
		const char *name;
		gboolean (*init) (struct plugin *);
		gboolean (*fini) (struct plugin *);
		gboolean (*save_config) (struct plugin *, xmlNodePtr);
		gboolean (*load_config) (struct plugin *, xmlNodePtr configuration);
	} *ops;
};

/* plugins.c */
G_MODULE_EXPORT struct plugin *load_plugin(struct plugin_config *);
G_MODULE_EXPORT gboolean unload_plugin(struct plugin *);
G_MODULE_EXPORT gboolean plugin_loaded(const char *name);
G_MODULE_EXPORT GList *get_plugin_list(void);

#if defined(_WIN32) && !defined(CTRLPROXY_CORE_BUILD)
G_MODULE_EXPORT gboolean fini_plugin(struct plugin *p);
G_MODULE_EXPORT gboolean init_plugin(struct plugin *p);
G_MODULE_EXPORT gboolean load_config(struct plugin *p, xmlNodePtr);
G_MODULE_EXPORT gboolean save_config(struct plugin *p, xmlNodePtr);
G_MODULE_EXPORT const char name_plugin[];
#pragma comment(lib,"ctrlproxy.lib")
#endif

#endif /* __CTRLPROXY_PLUGINS_H__ */
