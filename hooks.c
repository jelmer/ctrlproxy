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

struct filter_data {
	char *name;
	int priority;
	filter_function function;
	void *userdata;
};

static gint filter_cmp(gconstpointer _a, gconstpointer _b)
{
	struct filter_data *a = (struct filter_data *)_a;
	struct filter_data *b = (struct filter_data *)_b;

	return a->priority - b->priority;
}

static GList *add_filter_ex(GList *class, const char *name, filter_function f, void *userdata, int prio)
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
	GList *gl = list;

	while(gl) {
		struct filter_data *d = (struct filter_data *)gl->data;

		if(!g_strcasecmp(d->name, name)) 
		{
			g_free(d->name);
			g_free(d);
			return g_list_remove(list, d);
		}
	
		gl = gl->next;
	}

	return list;
}


static gboolean filter_class_execute(GList *gl, struct line *l) 
{
	while(gl) {
		struct filter_data *d = (struct filter_data *)gl->data;
		
		if(!d->function(l, d->userdata)) {
			return FALSE;
		}

		gl = gl->next;
	}
	
	return TRUE;
}

static GList *log_filters = NULL, 
			 *replication_filters = NULL, 
			 *client_filters = NULL, 
			 *server_filters = NULL;

#define FILTER_FUNCTIONS(n,list) \
void add_##n##_filter(const char *name, filter_function f, void *userdata, int priority)\
{\
	g_message(_(#n" filter '%s' added"), name); \
	list = add_filter_ex(list, name, f, userdata, priority);\
}\
\
void del_##n##_filter(const char *name)\
{\
	g_message(_(#n" filter '%s' removed"), name); \
	list = del_filter_ex(list, name); \
}\
gboolean run_##n##_filter(struct line *l)\
{\
/*	g_message(_("running filter class '"#n"'")); */\
	return filter_class_execute(list, l);\
}

FILTER_FUNCTIONS(log,log_filters)
FILTER_FUNCTIONS(replication,replication_filters)
FILTER_FUNCTIONS(client,client_filters)
FILTER_FUNCTIONS(server,server_filters)

/* Hooks that are called when a client is added or removed. 
 * Very useful for replication backends */

struct new_client_hook_data {
	char *name;
	new_client_hook hook;
	void *userdata;
};

GList *new_client_hooks = NULL;

struct lose_client_hook_data {
	char *name;
	lose_client_hook hook;
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
	
		if(!d->hook(c, d->userdata)) {
			g_message(_("New client hook '%s' refused new client"), d->name);
			return FALSE;
		}

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
		d->hook(c, d->userdata);
		l = l->next;
	}
}

struct motd_hook_data {
	char *name;
	motd_hook hook;
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

		nl = d->hook(n, d->userdata);

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
		d->hook(c, d->userdata);
		l = l->next;
	}
}

struct server_disconnected_hook_data {
	char *name;
	server_disconnected_hook hook;
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
		d->hook(c, d->userdata);
		l = l->next;
	}
}
