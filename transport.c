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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

GList *transports = NULL;
extern FILE *debugfd;

void register_transport(struct transport_ops *functions)
{
	struct transport_ops *functions1 = malloc(sizeof(struct transport_ops));
	memcpy(functions1, functions, sizeof(struct transport_ops));
	functions1->reference_count = 0;
	functions1->plugin = current_plugin;
	transports = g_list_append(transports, functions1);
}

gboolean unregister_transport(char *name)
{
	GList *gl = transports;
	while(gl) {
		struct transport_ops *t = (struct transport_ops *)gl->data;
		if(!strcmp(t->name, name)) {
			if(t->reference_count == 0) {
				transports = g_list_remove(transports, gl->data);
				free(t);
				return TRUE;
			} else {
				return FALSE; /* Transport is still in use */
			}
		}
		gl = gl->next;
	}
	return FALSE;
}

struct transport_context *transport_connect(const char *name, xmlNodePtr p, receive_handler r_h, connected_handler c_h, disconnect_handler d_h, void *d)
{
	struct transport_context *ret;
	struct transport_ops *t;
	GList *g = transports;

	while(g) {
		t = (struct transport_ops *)g->data;
		if(!strcmp(t->name, name))break;
		g = g->next;
	}

	if(!g) return NULL;

	t->reference_count++;
	
	ret = malloc(sizeof(struct transport_context));
	ret->configuration = p;
	ret->functions = t;
	ret->data = NULL;
	ret->caller_data = d;
	ret->on_new_client = NULL;
	ret->on_connect = c_h;
	ret->on_disconnect = d_h;
	ret->on_receive = r_h;
	
	if(!ret->functions->connect || ret->functions->connect(ret) == -1)
	{ 
		free(ret);
		return NULL;
	}
	
	return ret;
}

struct transport_context *transport_listen(const char *name, xmlNodePtr p, newclient_handler n_h, void *d)
{
	struct transport_context *ret;
	struct transport_ops *t;
	GList *g = transports;

	while(g) {
		t = (struct transport_ops *)g->data;
		if(!strcmp(t->name, name))break;
		g = g->next;
	}

	if(!g) return NULL;
	
	t->reference_count++;
	
	ret = malloc(sizeof(struct transport_context));
	ret->configuration = p;
	ret->functions = t;
	ret->data = NULL;
	ret->caller_data = d;
	ret->on_connect = NULL;
	ret->on_new_client = n_h;
	ret->on_disconnect = NULL;
	ret->on_receive = NULL;
	
	if(!ret->functions->listen || ret->functions->listen(ret) == -1)
	{ 
		free(ret);
		return NULL;
	}
	
	return ret;
}

void transport_free(struct transport_context *t)
{
	if(!t) return;
	t->functions->reference_count++;
	if(t->functions->close)	t->functions->close(t);
	free(t);
}

int transport_write(struct transport_context *t, char *l)
{
	if(debugfd)fprintf(debugfd, "[TO] %s\n", l);
	if(!t->functions->write)return -1;
	return t->functions->write(t, l);
}

void transport_set_disconnect_handler(struct transport_context *t, disconnect_handler d_h)
{
	t->on_disconnect = d_h;
}

void transport_set_receive_handler(struct transport_context *t, receive_handler r_h)
{
	t->on_receive = r_h;
}

void transport_set_newclient_handler(struct transport_context *t, newclient_handler n_h)
{
	t->on_new_client = n_h;
}

void transport_set_data(struct transport_context *t, void *data)
{
	t->caller_data = data;
}
