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

#include "ctrlproxy.h"
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <errno.h>

#include <glib/gstdio.h>
#include <sys/stat.h>

#define STATE_DUMP_INTERVAL 1000

struct lf_data {
	GIOChannel *line_file;
	FILE *state_file;
	int lines_since_last_state;
};

static void file_insert_state(struct linestack_context *ctx, const struct network_state *state);

static gboolean file_init(struct linestack_context *ctx, const char *name, struct ctrlproxy_config *config, const struct network_state *state)
{
	struct lf_data *data = g_new0(struct lf_data, 1);
	char *parent_dir, *data_dir, *data_file;
	GError *error = NULL;

	parent_dir = g_build_filename(config->config_dir, "linestack_file", NULL);
	g_mkdir(parent_dir, 0700);
	data_dir = g_build_filename(parent_dir, name, NULL);
	g_free(parent_dir);
	g_mkdir(data_dir, 0700);
	data_file = g_build_filename(data_dir, "lines", NULL);

	data->line_file = g_io_channel_new_file(data_file, "w+", &error);
	if (data->line_file == NULL) {
		log_global(NULL, LOG_WARNING, "Error opening `%s': %s", 
						  data_file, error->message);
		g_free(data_file);
		return FALSE;
	}
	g_free(data_file);

	data_file = g_build_filename(data_dir, "state", NULL);

	data->state_file = fopen(data_file, "w+");
	if (data->state_file == NULL) {
		log_global(NULL, LOG_WARNING, "Error opening `%s': %s", 
						  data_file, strerror(errno));
		g_free(data_file);
		return FALSE;
	}
	g_free(data_file);

	g_free(data_dir);
	ctx->backend_data = data;
	file_insert_state(ctx, state);
	return TRUE;
}

static gboolean file_fini(struct linestack_context *ctx)
{
	struct lf_data *data = ctx->backend_data;
	g_io_channel_unref(data->line_file);
	fclose(data->state_file);
	g_free(data);
	return TRUE;
}

static gint64 g_io_channel_tell_position(GIOChannel *gio)
{
	int fd = g_io_channel_unix_get_fd(gio);
	return lseek(fd, 0, SEEK_CUR);
}

static void file_insert_state(struct linestack_context *ctx, const struct network_state *state)
{
	size_t length;
	struct lf_data *nd = ctx->backend_data;
	char *raw = network_state_encode(state, &length);
	gint64 offset;

	log_network_state(NULL, LOG_TRACE, state, "Inserting state");
	
	nd->lines_since_last_state = 0;

	offset = g_io_channel_tell_position(nd->line_file);

	if (fwrite(&offset, sizeof(offset), 1, nd->state_file) != 1)
		return;

	if (fwrite(&length, sizeof(length), 1, nd->state_file) != 1)
		return;

	if (fwrite(raw, length, 1, nd->state_file) != 1)
		return;

	g_free(raw);

	return;
}

static gboolean file_insert_line(struct linestack_context *ctx, const struct line *l, const struct network_state *state)
{
	struct lf_data *nd = ctx->backend_data;
	char t[20];
	GError *error = NULL;
	GIOStatus status;
	
	if (nd == NULL) 
		return FALSE;

	nd->lines_since_last_state++;
	if (nd->lines_since_last_state == STATE_DUMP_INTERVAL) 
		file_insert_state(ctx, state);

	g_snprintf(t, sizeof(t), "%ld ", time(NULL));

	status = g_io_channel_write_chars(nd->line_file, t, strlen(t), NULL, &error);
	g_assert(status == G_IO_STATUS_NORMAL);

	if (!irc_send_line(nd->line_file, l))
		return FALSE;

	return TRUE;
}

static void *file_get_marker(struct linestack_context *ctx)
{
	long *pos;
	struct lf_data *nd = ctx->backend_data;

	pos = g_new0(long, 1);
	*pos = g_io_channel_tell_position(nd->line_file);
	return pos;
}

static struct network_state * file_get_state (
		struct linestack_context *ctx, 
		void *m)
{
	struct lf_data *nd = ctx->backend_data;
	struct network_state *ret;
	char *raw;
	long from_offset, *to_offset = m;
	struct linestack_marker m1, m2;
	GError *error = NULL;
	GIOStatus status;

	if (!nd) 
		return NULL;

	/* Flush channel before reading otherwise data corruption may occur */
	g_io_channel_flush(nd->line_file, &error);
	fflush(nd->state_file);

	/* FIXME: Search back from end of the state file to begin 
	 * and find the state dump with the highest offset but an offset 
	 * below from_offset */

#if 0
	raw = g_malloc(rh.length+1);

	if (fread(raw, rh.length, 1, nd->state_file) != 1)
		return FALSE;

	raw[rh.length] = '\0';

	ret = network_state_decode(raw, rh.length, NULL);

	g_free(raw);
#endif
	m1.data = &from_offset;
	m2.data = to_offset;
	linestack_replay(ctx, &m1, &m2, ret);

	status = g_io_channel_seek_position(nd->line_file, 0, G_SEEK_END, &error);
	g_assert(status == G_IO_STATUS_NORMAL);
	
	return ret;
}

static gboolean file_traverse(struct linestack_context *ctx,
		void *mf,
		void *mt,
		linestack_traverse_fn tf, 
		void *userdata)
{
	long *start_offset, *end_offset;
	struct lf_data *nd = ctx->backend_data;
	GError *error = NULL;
	GIOStatus status;
	char *raw, *space;
	struct line *l;

	if (nd == NULL) 
		return FALSE;

	/* Flush channel before reading otherwise data corruption may occur */
	g_io_channel_flush(nd->line_file, &error);
	fflush(nd->state_file);
	
	start_offset = mf;
	end_offset = mt;

	/* Go back to begin of file */
	status = g_io_channel_seek_position(nd->line_file, 
			start_offset?*start_offset:0, G_SEEK_SET, &error);

	g_assert (status == G_IO_STATUS_NORMAL);
	
	while((status = g_io_channel_read_line(nd->line_file, &raw, NULL, 
		                          NULL, &error) != G_IO_STATUS_EOF)) {
		if (status != G_IO_STATUS_NORMAL) {
			log_global(NULL, LOG_WARNING, "read_line() failed: %s",
					error->message);
			return FALSE;
		}

		space = strchr(raw, ' ');
		*space = '\0';

		g_assert(space);
	
		l = irc_parse_line(space+1);
		tf(l, atol(raw), userdata);
		free_line(l);

		g_free(raw);
	}

	status = g_io_channel_seek_position(nd->line_file, 0, G_SEEK_END, 
	                                    &error);

	g_assert(status == G_IO_STATUS_NORMAL);

	return TRUE;
}

const struct linestack_ops linestack_file = {
	.name = "file",
	.init = file_init,
	.fini = file_fini,
	.insert_line = file_insert_line,
	.get_marker = file_get_marker,
	.get_state = file_get_state, 
	.free_marker = g_free,
	.traverse = file_traverse
};
