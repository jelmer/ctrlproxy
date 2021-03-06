/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2005 Jelmer Vernooĳ <jelmer@jelmer.uk>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
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

/**
 * @file
 * @brief Plugins
 */

/**
 * A plugin.
 */
struct plugin {
	/** Plugin handle */
	GModule *module;
	/** Plugin private data. */
	void *data;
	/** Plugin operations */
	struct plugin_ops *ops;
};

/* plugins.c */
G_MODULE_EXPORT struct plugin *load_plugin(const char *dir, const char *name);
G_MODULE_EXPORT gboolean plugin_loaded(const char *name);
G_MODULE_EXPORT GList *get_plugin_list(void);

#if defined(_WIN32) && !defined(CTRLPROXY_CORE_BUILD)
G_MODULE_EXPORT gboolean init_plugin(struct plugin *p);
G_MODULE_EXPORT gboolean load_config(struct plugin *p, xmlNodePtr);
G_MODULE_EXPORT gboolean save_config(struct plugin *p, xmlNodePtr);
G_MODULE_EXPORT const char name_plugin[];
#pragma comment(lib,"ctrlproxy.lib")
#endif

#endif /* __CTRLPROXY_PLUGINS_H__ */
