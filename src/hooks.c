/*
	ctrlproxy: A modular IRC proxy
	(c) 2003 Jelmer Vernooĳ <jelmer@jelmer.uk>

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

#include "internals.h"

struct filter_data {
	char *name;
	int priority;
	server_filter_function function;
	void *userdata;
};

static gint filter_cmp(gconstpointer _a, gconstpointer _b)
{
	struct filter_data *a = (struct filter_data *)_a;
	struct filter_data *b = (struct filter_data *)_b;

	return a->priority - b->priority;
}

static GList *add_filter_ex(GList *class, const char *name, server_filter_function f, void *userdata, int prio)
{
	struct filter_data *d = (struct filter_data *)g_malloc(sizeof(struct filter_data));

	d->name = g_strdup(name);
	d->function = f;
	d->priority = prio;
	d->userdata = userdata;

	return g_list_insert_sorted(class, d, filter_cmp);
}

static GList *del_filter_ex(GList *list, const char *name)
{
	GList *gl;

	for (gl = list; gl; gl = gl->next) {
		struct filter_data *d = (struct filter_data *)gl->data;

		if (!strcmp(d->name, name)) {
			g_free(d->name);
			g_free(d);
			return g_list_remove(list, d);
		}
	}

	return list;
}


static gboolean filter_class_execute(GList *gl, struct irc_network *s, enum data_direction dir, const struct irc_line *l)
{
	while(gl) {
		struct filter_data *d = (struct filter_data *)gl->data;

		if (!d->function(s, l, dir, d->userdata)) {
			return FALSE;
		}

		gl = gl->next;
	}

	return TRUE;
}

static GList *log_filters = NULL,
			 *server_filters = NULL;

#define FILTER_FUNCTIONS(n,list) \
void add_##n##_filter(const char *name, server_filter_function f, void *userdata, int priority)\
{\
	list = add_filter_ex(list, name, f, userdata, priority);\
}\
\
void del_##n##_filter(const char *name)\
{\
	list = del_filter_ex(list, name); \
}\
gboolean run_##n##_filter(struct irc_network *s, const struct irc_line *l, enum data_direction dir)\
{\
	return filter_class_execute(list, s, dir, l);\
}

FILTER_FUNCTIONS(log,log_filters)
FILTER_FUNCTIONS(server,server_filters)

/* Hooks that are called when a client is added or removed. */

struct new_client_hook_data {
	char *name;
	new_client_hook hook;
	void *userdata;
};

GList *new_client_hooks = NULL;

void add_new_client_hook(const char *name, new_client_hook h, void *userdata)
{
	struct new_client_hook_data *d;

	d = g_malloc(sizeof(struct new_client_hook_data));
	d->name = g_strdup(name);
	d->userdata = userdata;
	d->hook = h;
	new_client_hooks = g_list_append(new_client_hooks, d);
}

void del_new_client_hook(const char *name)
{
	GList *l;

	for (l = new_client_hooks; l; l = l->next) {
		struct new_client_hook_data *d = (struct new_client_hook_data *)l->data;
		if (!strcmp(d->name, name)) {
			g_free(d->name);
			g_free(d);
			new_client_hooks = g_list_remove(new_client_hooks, d);
			return;
		}
	}
}

gboolean new_client_hook_execute(struct irc_client *c)
{
	GList *l;

	for (l = new_client_hooks; l; l = l->next) {
		struct new_client_hook_data *d = (struct new_client_hook_data *)l->data;

		if (!d->hook(c, d->userdata)) {
			g_debug(("New client hook '%s' refused new client"), d->name);
			return FALSE;
		}
	}

	return TRUE;
}
