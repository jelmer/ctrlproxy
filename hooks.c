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
