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
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

struct file_information {
	GIOChannel *channel;
	char *filename;
};

static gboolean file_init(struct linestack_context *c, char *args)
{
	struct file_information *d = malloc(sizeof(struct file_information));
	GError *error = NULL;
	if(args) {
		d->channel = g_io_channel_new_file(args, "a+", NULL);
		d->filename = strdup(args);
	} else  {
		int tmpfd;
		char *p = ctrlproxy_path("linestack_file");
		mkdir(p, 0700);
		asprintf(&d->filename, "%s/XXXXXX", p);
		free(p);
		tmpfd = g_mkstemp(d->filename);
		if(tmpfd < 0) return FALSE;

		d->channel = g_io_channel_unix_new(tmpfd);
	}

	g_io_channel_set_encoding(d->channel, NULL, &error);

	c->data = d;

	if(!c->data || !d->channel) return FALSE;
	return TRUE;
}

static gboolean file_clear(struct linestack_context *c)
{
	struct file_information *d = (struct file_information *)c->data;
	g_io_channel_seek_position(d->channel, 0, G_SEEK_SET, NULL);
	return TRUE;
}

static gboolean file_close(struct linestack_context *c)
{
	struct file_information *d = (struct file_information *)c->data;
	g_io_channel_shutdown(d->channel, TRUE, NULL);
	unlink(d->filename);
	free(d->filename);
	free(d);
	c->data = NULL;
	return TRUE;
}

static GSList *file_get_linked_list(struct linestack_context *c)
{
	struct file_information *d = (struct file_information *)c->data;
	GError *error = NULL;
	GSList *ret = NULL;
	char *raw;

	/* Go back to begin of file */
	g_io_channel_seek(d->channel, 0, G_SEEK_SET);
	
	while(g_io_channel_read_line(d->channel, &raw, NULL, NULL, &error) == G_IO_STATUS_NORMAL) {
		struct line *l;
		l = irc_parse_line(raw);
		free(raw);

		ret = g_slist_append(ret, l);
	}

	return ret;
}

static gboolean file_add_line(struct linestack_context *b, struct line *l)
{
	struct file_information *d = (struct file_information *)b->data;
	GError *error = NULL;
	char *raw = irc_line_string_nl(l);
	g_assert(d->channel);
	g_io_channel_write_chars(d->channel, raw, -1, NULL, &error);
	free(raw);
	return TRUE;
}	

static void file_send_file(struct linestack_context *b, struct transport_context *t) 
{
	struct file_information *d = (struct file_information *)b->data;
	char *raw;
	GError *error = NULL;
	
	/* Go back to begin of file */
	g_io_channel_seek(d->channel, 0, G_SEEK_SET);
	
	while(g_io_channel_read_line(d->channel, &raw, NULL, NULL, &error) == G_IO_STATUS_NORMAL)  {
		transport_write(t, raw);
		free(raw);
	}
}

static struct linestack_ops file = {
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
