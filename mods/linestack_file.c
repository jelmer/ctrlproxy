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

gboolean file_init(struct linestack_context *c, char *args)
{
	c->data = fopen(args, "a+");
	if(!c->data) return FALSE;
	return TRUE;
}

gboolean file_clear(struct linestack_context *c)
{
	FILE *f = (FILE *)c->data;
	rewind(f);
	return TRUE;
}

gboolean file_close(struct linestack_context *c)
{
	FILE *f = (FILE *)c->data;
	fclose(f);
	return TRUE;
}

GSList *file_get_linked_list(struct linestack_context *c)
{
	FILE *f = (FILE *)c->data;
	GSList *ret = NULL;
	
	while(!feof(f)) {
		char *raw;
		struct line *l;
		if(fscanf(f, "%a\r\n", &raw) != 1) break;

		l = irc_parse_line(raw);

		free(raw);

		ret = g_slist_append(ret, l);
	}

	return ret;
}

gboolean file_add_line(struct linestack_context *b, struct line *l)
{
	FILE *f = (FILE *)b->data;
	char *raw = irc_line_string_nl(l);
	fputs(raw, f);
	free(raw);
	return TRUE;
}	

void file_send_file(struct linestack_context *b, struct transport_context *t) 
{
	FILE *f = (FILE *)b->data;
	
	while(!feof(f)) {
		char *raw;
		if(fscanf(f, "%a\r\n", &raw) != 1) break;

		transport_write(t, raw);
		free(raw);
	}
}

struct linestack file = {
	"file",
	file_init,
	file_clear,
	file_add_line,
	file_get_linked_list,
	file_send_file, 
	file_close
};

gboolean fini_plugin(struct plugin *p) {
	unregister_linestack(&file);
	return TRUE;
}

gboolean init_plugin(struct plugin *p) {
	register_linestack(&file);
	return TRUE;
}
