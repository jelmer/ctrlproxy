/* 
	ctrlproxy: A modular IRC proxy
	(c) 2002-2003 Jelmer Vernooij <jelmer@nl.linux.org>

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

#define _GNU_SOURCE
#include "ctrlproxy.h"
#include <string.h>

static gboolean none_replicate(struct client *c)
{
	GSList *d, *orig = gen_replication_network(c->network);
	d = orig;
	while(d) {
		struct line *l = (struct line *)d->data;
		irc_send_line(c->incoming, l);
		free_line(l);
		d = d->next;
	}
	g_slist_free(orig);
	return TRUE;
}

gboolean fini_plugin(struct plugin *p) {
	del_new_client_hook("repl_none");
	return TRUE;
}

gboolean init_plugin(struct plugin *p) {
	add_new_client_hook("repl_none", none_replicate);
	return TRUE;
}
