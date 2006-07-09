/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2006 Jelmer Vernooij <jelmer@nl.linux.org>

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "internals.h"

static GList *load_notifies = NULL, *save_notifies = NULL;

void register_load_config_notify(config_load_notify_fn fn)
{
	load_notifies = g_list_append(load_notifies, fn);
}

void register_save_config_notify(config_save_notify_fn fn)
{
	save_notifies = g_list_append(save_notifies, fn);
}

void config_load_notify(struct global *global)
{
	GList *gl;
	for (gl = load_notifies; gl; gl = gl->next) {
		config_load_notify_fn fn = gl->data;

		fn(global);
	}
}

void config_save_notify(struct global *global, const char *dest)
{
	GList *gl;
	for (gl = save_notifies; gl; gl = gl->next) {
		config_save_notify_fn fn = gl->data;

		fn(global, dest);
	}
}

/* globals */
struct global *my_global;

struct global *new_global(const char *config_dir)
{
	struct global *global = g_new0(struct global, 1);

	global->config = load_configuration(config_dir);

	if (!global->config) {
		g_free(global);
		return NULL;
	}

	load_networks(global, global->config);

	nickserv_load(global);

	config_load_notify(global);
	
	return global;
}

void free_global(struct global *global)
{
	if (global == NULL)
		return;
	fini_networks(global);
	free_config(global->config);
	global->config = NULL;
	fini_networks(global);
}

