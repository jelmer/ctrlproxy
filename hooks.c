/* 
	ctrlproxy: A modular IRC proxy
	(c) 2003 Jelmer Vernooij <jelmer@nl.linux.org>

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

GList *filter_classes = NULL;

struct filter_class {
	char *name;
	int priority;
	GList *filters;
};

struct filter_data {
	char *name;
	int priority;
	filter_function function;
	struct plugin *plugin;
	void *userdata;
};

static gint filter_cmp(gconstpointer _a, gconstpointer _b)
{
	struct filter_data *a = (struct filter_data *)_a;
	struct filter_data *b = (struct filter_data *)_b;

	return a->priority - b->priority;
}

static gint filter_class_cmp(gconstpointer _a, gconstpointer _b)
{
	struct filter_class *a = (struct filter_class *)_a;
	struct filter_class *b = (struct filter_class *)_b;

	return a->priority - b->priority;
}

static struct filter_class *find_filter_class(char *name)
{
	GList *gl = filter_classes;
	while(gl) {
		struct filter_class *c = (struct filter_class *)gl->data;
		if(!strcmp(c->name, name)) return c;
		gl = gl->next;
	}
	return NULL;
}

void add_filter_class(char *name, int prio)
{
	struct filter_class *c;
	
	if(find_filter_class(name)) return;
	
	c = (struct filter_class *)g_malloc(sizeof(struct filter_class));
	c->name = g_strdup(name);
	
	c->filters = NULL;

	c->priority = prio;
	
	filter_classes = g_list_insert_sorted(filter_classes, c, filter_class_cmp);
}

gboolean add_filter_ex(char *name, filter_function f, void *userdata, char *class, int prio)
{
	struct filter_data *d = (struct filter_data *)g_malloc(sizeof(struct filter_data));
	struct filter_class *c;

	d->name = g_strdup(name);
	g_message(_("Filter '%s' added"), d->name);
	d->function = f;
	d->priority = prio;
	d->plugin = peek_plugin();
	d->userdata = userdata;

	c = find_filter_class(class);
	if(!c) {
		g_message(_("Unable to find filter class '%s'"), class);
		return FALSE;
	}
	
	c->filters = g_list_insert_sorted(c->filters, d, filter_cmp);
	return TRUE;
}

G_MODULE_EXPORT void add_filter(char *name, filter_function f, void *userdata) 
{
	add_filter_ex(name, f, userdata, "", 500);
}

void del_filter(filter_function f)
{
	del_filter_ex("", f);
}

gboolean del_filter_ex(char *class, filter_function f) 
{
	struct filter_class *c = find_filter_class(class);
	GList *gl;
	if(!c) return FALSE;
	gl = c->filters;
	while(gl) {
		struct filter_data *d = (struct filter_data *)gl->data;

		if(d->function == f) 
		{
			g_message(_("Filter '%s' removed"), d->name);
			c->filters = g_list_remove(c->filters, d);
			g_free(d->name);
			g_free(d);
			return TRUE;
		}
	
		gl = gl->next;
	}
	return FALSE;
}


static gboolean filter_class_execute(struct filter_class *c, struct line *l) 
{
	GList *gl = c->filters;
	while(gl) {
		struct filter_data *d = (struct filter_data *)gl->data;
		
		push_plugin(d->plugin);
		
		if(!d->function(l, d->userdata)) {
			pop_plugin();
			return FALSE;
		}
		pop_plugin();

		gl = gl->next;
	}
	
	return TRUE;
}

gboolean filters_execute_class(char *name, struct line *l)
{
	struct filter_class *c = find_filter_class(name);
	if(!c) return FALSE;

	return filter_class_execute(c, l);
}

gboolean filters_execute(struct line *l) 
{
	GList *cl = filter_classes;
	struct filter_class *c;
	c = find_filter_class("");
	if(!filter_class_execute(c, l)) return FALSE;
	
	while(cl) {
		c = (struct filter_class *)cl->data;
		if(!strcmp(c->name, "client")) {
			filter_class_execute(c, l);
		} else if(strcmp(c->name,"")) {
			struct line *tl = linedup(l);
			filter_class_execute(c, tl);
			free_line(tl);
		}
		cl = cl->next;
	}

	return TRUE;
}

/* Hooks that are called when a client is added or removed. 
 * Very useful for replication backends */

struct new_client_hook_data {
	char *name;
	new_client_hook hook;
	struct plugin *plugin;
	void *userdata;
};

GList *new_client_hooks = NULL;

struct lose_client_hook_data {
	char *name;
	lose_client_hook hook;
	struct plugin *plugin;
	void *userdata;
};

GList *lose_client_hooks = NULL;

void add_new_client_hook(char *name, new_client_hook h, void *userdata)
{
	struct new_client_hook_data *d;
	
	d = g_malloc(sizeof(struct new_client_hook_data));
	d->name = g_strdup(name);
	d->userdata = userdata;
	g_message(_("Adding new client hook '%s'"), d->name);
	d->hook = h;
	d->plugin = peek_plugin();
	new_client_hooks = g_list_append(new_client_hooks, d);
}

void del_new_client_hook(char *name)
{
	GList *l = new_client_hooks;
	while(l) {
		struct new_client_hook_data *d = (struct new_client_hook_data *)l->data;
		if(!strcmp(d->name, name)) {
			g_message(_("New client hook '%s' removed"), d->name);
			g_free(d->name);
			g_free(d);
			new_client_hooks = g_list_remove(new_client_hooks, d);
			return;
		}
		l = l->next;
	}
}

gboolean new_client_hook_execute(struct client *c)
{
	GList *l = new_client_hooks;
	while(l) {
		struct new_client_hook_data *d = (struct new_client_hook_data *)l->data;
		push_plugin(d->plugin);
	
		if(!d->hook(c, d->userdata)) {
			g_message(_("New client hook '%s' refused new client"), d->name);
			pop_plugin();
			return FALSE;
		}
		pop_plugin();

		l = l->next;
	}

	return TRUE;
}

void add_lose_client_hook(char *name, lose_client_hook h, void *userdata)
{
	struct lose_client_hook_data *d;
	g_message(_("Adding lose client hook '%s'"), name);
	
	d = g_malloc(sizeof(struct lose_client_hook_data));
	d->name = g_strdup(name);
	d->hook = h;
	d->userdata = userdata;
	d->plugin = peek_plugin();
	lose_client_hooks = g_list_append(lose_client_hooks, d);
}

void del_lose_client_hook(char *name)
{
	GList *l = lose_client_hooks;
	while(l) {
		struct lose_client_hook_data *d = (struct lose_client_hook_data *)l->data;
		if(!strcmp(d->name, name)) {
			g_message(_("Lose client hook '%s' removed"), d->name);
			g_free(d->name);
			lose_client_hooks = g_list_remove(lose_client_hooks, d);
			return;
		}
		l = l->next;
	}
}

void lose_client_hook_execute(struct client *c)
{
	GList *l = lose_client_hooks;
	while(l) {
		struct lose_client_hook_data *d = (struct lose_client_hook_data *)l->data;
		push_plugin(d->plugin);
		d->hook(c, d->userdata);
		pop_plugin();
		l = l->next;
	}
}

struct motd_hook_data {
	char *name;
	motd_hook hook;
	struct plugin *plugin;
	void *userdata;
};

GList *motd_hooks = NULL;

void add_motd_hook(char *name, motd_hook h, void *userdata)
{
	struct motd_hook_data *d;
	g_message(_("Adding MOTD hook '%s'"), name);

	d = g_malloc(sizeof(struct motd_hook_data));
	d->name = g_strdup(name);
	d->hook = h;
	d->userdata = userdata;
	d->plugin = peek_plugin();

	motd_hooks = g_list_append(motd_hooks, d);
}

void del_motd_hook(char *name)
{
	GList *l = motd_hooks;
	while(l) {
		struct motd_hook_data *d = (struct motd_hook_data *)l->data;
		if(!strcmp(d->name, name)) {
			g_message(_("MOTD hook '%s' removed"), d->name);
			g_free(d->name);
			g_free(d);
			motd_hooks = g_list_remove(motd_hooks, d);
			return;
		}
		l = l->next;
	}
}

char ** get_motd_lines(struct network *n)
{
	char **l = g_malloc(sizeof(char *));
	size_t curnum = 0;
	GList *gl = motd_hooks;
	while(gl) {
		char **nl;
		int i,j;
		struct motd_hook_data *d = (struct motd_hook_data *)gl->data;

		push_plugin(d->plugin);
		nl = d->hook(n, d->userdata);
		pop_plugin();

		if(!nl) { 
			gl = gl->next;
			continue;
		}

		/* Count number of added lines */
		for(i = 0; nl[i]; i++);

		l = g_realloc(l, (curnum+i+1) * sizeof(char *));

		for(j = 0; j < i; j++) l[curnum+j] = nl[j];

		g_free(nl);

		curnum+=i;
		
		gl = gl->next;
	}

	l[curnum] = NULL;
	
	if(!l[0]) { 
		g_free(l);
		return NULL;
	}

	return l;
}

struct server_connected_hook_data {
	char *name;
	server_connected_hook hook;
	struct plugin *plugin;
	void *userdata;
};

GList *server_connected_hooks = NULL;

void add_server_connected_hook(char *name, server_connected_hook h, void *userdata)
{
	struct server_connected_hook_data *d;
	g_message(_("Adding lose client hook '%s'"), name);
	
	d = g_malloc(sizeof(struct server_connected_hook_data));
	d->name = g_strdup(name);
	d->hook = h;
	d->userdata = userdata;
	server_connected_hooks = g_list_append(server_connected_hooks, d);
}

void del_server_connected_hook(char *name)
{
	GList *l = server_connected_hooks;
	while(l) {
		struct server_connected_hook_data *d = (struct server_connected_hook_data *)l->data;
		if(!strcmp(d->name, name)) {
			g_message(_("Lose client hook '%s' removed"), d->name);
			g_free(d->name);
			g_free(d);
			server_connected_hooks = g_list_remove(server_connected_hooks, d);
			return;
		}
		l = l->next;
	}
}

void server_connected_hook_execute(struct network *c)
{
	GList *l = server_connected_hooks;
	while(l) {
		struct server_connected_hook_data *d = (struct server_connected_hook_data *)l->data;
		push_plugin(d->plugin);
		d->hook(c, d->userdata);
		pop_plugin();
		l = l->next;
	}
}

struct server_disconnected_hook_data {
	char *name;
	server_disconnected_hook hook;
	struct plugin *plugin;
	void *userdata;
};

GList *server_disconnected_hooks = NULL;

void add_server_disconnected_hook(char *name, server_disconnected_hook h, void *userdata)
{
	struct server_disconnected_hook_data *d;
	g_message(_("Adding lose client hook '%s'"), name);
	
	d = g_malloc(sizeof(struct server_disconnected_hook_data));
	d->name = g_strdup(name);
	d->hook = h;
	d->userdata = userdata;
	d->plugin = peek_plugin();
	server_disconnected_hooks = g_list_append(server_disconnected_hooks, d);
}

void del_server_disconnected_hook(char *name)
{
	GList *l = server_disconnected_hooks;
	while(l) {
		struct server_disconnected_hook_data *d = (struct server_disconnected_hook_data *)l->data;
		if(!strcmp(d->name, name)) {
			g_message(_("Lose client hook '%s' removed"), d->name);
			g_free(d->name);
			g_free(d);
			server_disconnected_hooks = g_list_remove(server_disconnected_hooks, d);
			return;
		}
		l = l->next;
	}
}

void server_disconnected_hook_execute(struct network *c)
{
	GList *l = server_disconnected_hooks;
	while(l) {
		struct server_disconnected_hook_data *d = (struct server_disconnected_hook_data *)l->data;
		push_plugin(d->plugin);
		d->hook(c, d->userdata);
		pop_plugin();
		l = l->next;
	}
}
