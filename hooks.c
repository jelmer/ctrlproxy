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

GList *filters = NULL;

struct filter_data {
	char *name;
	filter_function function;
};

void add_filter(char *name, filter_function f) 
{
	struct filter_data *d = (struct filter_data *)malloc(sizeof(struct filter_data));

	d->name = strdup(name);
	g_message("Filter '%s' added", d->name);
	d->function = f;

	filters = g_list_append(filters, d);
}

void del_filter(filter_function f) 
{
	GList *gl = filters;
	while(gl) {
		struct filter_data *d = (struct filter_data *)gl->data;

		if(d->function == f) 
		{
			g_message("Filter '%s' removed", d->name);
			filters = g_list_remove(filters, d);
			free(d->name);
			free(d);
			return;
		}
	
		gl = gl->next;
	}
}

gboolean filters_execute(struct line *l) 
{
	GList *gl = filters;
	while(gl) {
		struct filter_data *d = (struct filter_data *)gl->data;

		if(!d->function(l))return FALSE;

		gl = gl->next;
	}

	return TRUE;
}

/* Hooks that are called when a client is added or removed. 
 * Very useful for replication backends */

struct new_client_hook_data {
	char *name;
	new_client_hook hook;
};

GList *new_client_hooks = NULL;

struct lose_client_hook_data {
	char *name;
	lose_client_hook hook;
};

GList *lose_client_hooks = NULL;

void add_new_client_hook(char *name, new_client_hook h)
{
	struct new_client_hook_data *d;
	
	d = malloc(sizeof(struct new_client_hook_data));
	d->name = strdup(name);
	g_message("Adding new client hook '%s'", d->name);
	d->hook = h;
	new_client_hooks = g_list_append(new_client_hooks, d);
}

void del_new_client_hook(char *name)
{
	GList *l = new_client_hooks;
	while(l) {
		struct new_client_hook_data *d = (struct new_client_hook_data *)l->data;
		if(!strcmp(d->name, name)) {
			g_message("New client hook '%s' removed", d->name);
			free(d->name);
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
		if(!d->hook(c)) {
			g_message("New client hook '%s' refused new client", d->name);
			return FALSE;
		}

		l = l->next;
	}

	return TRUE;
}



void add_lose_client_hook(char *name, lose_client_hook h)
{
	struct lose_client_hook_data *d;
	g_message("Adding lose client hook '%s'", name);
	
	d = malloc(sizeof(struct lose_client_hook_data));
	d->name = strdup(name);
	d->hook = h;
	lose_client_hooks = g_list_append(lose_client_hooks, d);
}

void del_lose_client_hook(char *name)
{
	GList *l = lose_client_hooks;
	while(l) {
		struct lose_client_hook_data *d = (struct lose_client_hook_data *)l->data;
		if(!strcmp(d->name, name)) {
			g_message("Lose client hook '%s' removed", d->name);
			free(d->name);
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
		d->hook(c);
		l = l->next;
	}
}

struct motd_hook_data {
	char *name;
	motd_hook hook;
};

GList *motd_hooks = NULL;

void add_motd_hook(char *name, motd_hook h)
{
	struct motd_hook_data *d;
	g_message("Adding MOTD hook '%s'", name);

	d = malloc(sizeof(struct motd_hook_data));
	d->name = strdup(name);
	d->hook = h;

	motd_hooks = g_list_append(motd_hooks, d);
}

void del_motd_hook(char *name)
{
	GList *l = motd_hooks;
	while(l) {
		struct motd_hook_data *d = (struct motd_hook_data *)l->data;
		if(!strcmp(d->name, name)) {
			g_message("MOTD hook '%s' removed", d->name);
			free(d->name);
			motd_hooks = g_list_remove(motd_hooks, d);
			return;
		}
		l = l->next;
	}
}

char ** get_motd_lines(struct network *n)
{
	char **l = malloc(sizeof(char *));
	size_t curnum = 0;
	GList *gl = motd_hooks;
	while(gl) {
		char **nl;
		int i,j;
		struct motd_hook_data *d = (struct motd_hook_data *)gl->data;

		nl = d->hook(n);

		if(!nl) { 
			gl = gl->next;
			continue;
		}

		/* Count number of added lines */
		for(i = 0; nl[i]; i++);

		l = realloc(l, (curnum+i+1) * sizeof(char *));

		for(j = 0; j < i; j++) l[curnum+j] = nl[j];

		free(nl);

		curnum+=i;
		
		gl = gl->next;
	}

	l[curnum] = NULL;

	if(!l[0]) { 
		free(l);
		return NULL;
	}

	return l;
}

struct server_connected_hook_data {
	char *name;
	server_connected_hook hook;
};

GList *server_connected_hooks = NULL;

void add_server_connected_hook(char *name, server_connected_hook h)
{
	struct server_connected_hook_data *d;
	g_message("Adding lose client hook '%s'", name);
	
	d = malloc(sizeof(struct server_connected_hook_data));
	d->name = strdup(name);
	d->hook = h;
	server_connected_hooks = g_list_append(server_connected_hooks, d);
}

void del_server_connected_hook(char *name)
{
	GList *l = server_connected_hooks;
	while(l) {
		struct server_connected_hook_data *d = (struct server_connected_hook_data *)l->data;
		if(!strcmp(d->name, name)) {
			g_message("Lose client hook '%s' removed", d->name);
			free(d->name);
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
		d->hook(c);
		l = l->next;
	}
}

struct server_disconnected_hook_data {
	char *name;
	server_disconnected_hook hook;
};

GList *server_disconnected_hooks = NULL;

void add_server_disconnected_hook(char *name, server_disconnected_hook h)
{
	struct server_disconnected_hook_data *d;
	g_message("Adding lose client hook '%s'", name);
	
	d = malloc(sizeof(struct server_disconnected_hook_data));
	d->name = strdup(name);
	d->hook = h;
	server_disconnected_hooks = g_list_append(server_disconnected_hooks, d);
}

void del_server_disconnected_hook(char *name)
{
	GList *l = server_disconnected_hooks;
	while(l) {
		struct server_disconnected_hook_data *d = (struct server_disconnected_hook_data *)l->data;
		if(!strcmp(d->name, name)) {
			g_message("Lose client hook '%s' removed", d->name);
			free(d->name);
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
		d->hook(c);
		l = l->next;
	}
}
