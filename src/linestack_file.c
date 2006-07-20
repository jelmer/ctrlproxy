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
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <errno.h>

#include <glib/gstdio.h>
#include <sys/stat.h>

#define STATE_DUMP_INTERVAL 1000


struct record_header {
	guint32 offset;
	enum { RECORD_STATE, RECORD_LINE } type;
	time_t time;
	guint32 length;
};

struct lf_data {
	FILE *file;
	long last_state_offset;
	int lines_since_last_state;
	long last_marker_offset;
};

static void file_insert_state(struct linestack_context *ctx, const struct network_state *state);

static gboolean file_init(struct linestack_context *ctx, const char *name, struct ctrlproxy_config *config, const struct network_state *state)
{
	struct lf_data *data = g_new0(struct lf_data, 1);
	char *data_dir, *data_file;

	data_dir = g_build_filename(config->config_dir, "linestack_file", NULL);
	g_mkdir(data_dir, 0700);
	data_file = g_build_filename(data_dir, name, NULL);
	g_free(data_dir);

	data->file = fopen(data_file, "w+");
	if (data->file == NULL) {
		log_global(NULL, LOG_WARNING, "Error opening `%s': %s", 
						  data_file, strerror(errno));
		g_free(data_file);
		return FALSE;
	}
	g_free(data_file);
	ctx->backend_data = data;
	file_insert_state(ctx, state);
	return TRUE;
}

static gboolean file_fini(struct linestack_context *ctx)
{
	struct lf_data *data = ctx->backend_data;
	fclose(data->file);
	g_free(data);
	return TRUE;
}

static void file_insert_state(struct linestack_context *ctx, const struct network_state *state)
{
	size_t length;
	struct lf_data *nd = ctx->backend_data;
	char *raw = network_state_encode(state, &length);
	struct record_header rh;

	log_network_state(NULL, LOG_TRACE, state, "Inserting state");
	
	rh.time = time(NULL);
	rh.type = RECORD_STATE;
	rh.length = length;
	rh.offset = 0;

	nd->last_state_offset = ftell(nd->file);
	nd->lines_since_last_state = 0;

	if (fwrite(&rh, sizeof(rh), 1, nd->file) != 1)
		return;

	if (fwrite(raw, length, 1, nd->file) != 1)
		return;

	g_free(raw);

	return;
}

static gboolean file_insert_line(struct linestack_context *ctx, const struct line *l, const struct network_state *state)
{
	struct lf_data *nd = ctx->backend_data;
	char *raw;
	int ret;
	struct record_header rh;
	
	if (nd == NULL) 
		return FALSE;

	nd->lines_since_last_state++;
	if (nd->lines_since_last_state == STATE_DUMP_INTERVAL) 
		file_insert_state(ctx, state);

	rh.time = time(NULL);
	rh.type = RECORD_LINE;
	rh.offset = nd->last_state_offset;
	raw = irc_line_string_nl(l);
	rh.length = strlen(raw);

	nd->last_marker_offset = ftell(nd->file);
	
	if (fwrite(&rh, sizeof(rh), 1, nd->file) != 1)
		return FALSE;

	ret = fputs(raw, nd->file);
	g_free(raw);

	return (ret != EOF);
}

static void *file_get_marker(struct linestack_context *ctx)
{
	long *pos;
	struct lf_data *nd = ctx->backend_data;

	pos = g_new0(long, 1);
	*pos = nd->last_marker_offset;
	return pos;
}

static struct network_state * file_get_state (
		struct linestack_context *ctx, 
		void *m)
{
	struct lf_data *nd = ctx->backend_data;
	struct record_header rh;
	struct network_state *ret;
	char *raw;
	long from_offset, *to_offset = m;
	long save_offset;
	struct linestack_marker m1, m2;

	if (!nd) 
		return NULL;

	/* Flush channel before reading otherwise data corruption may occur */
	fflush(nd->file);

	save_offset = ftell(nd->file);

	if (to_offset) {
		fseek(nd->file, *to_offset, SEEK_SET);
		
		/* Read offset at marker position */
		if (fread(&rh, sizeof(rh), 1, nd->file) != 1) {
			log_global(NULL, LOG_WARNING, "Unable to fread at %ld", *to_offset);
			return NULL;
		}

		from_offset = rh.offset;
	} else {
		from_offset = nd->last_state_offset;
	}

	log_global(NULL, LOG_TRACE, "Reading state at 0x%04x (for 0x%04x)", from_offset, to_offset?*to_offset:-1);

	/* fseek to state dump */
	fseek(nd->file, from_offset, SEEK_SET);
	
	if (fread(&rh, sizeof(rh), 1, nd->file) != 1)
		return NULL;

	g_assert(rh.offset == 0);
	g_assert(rh.type == RECORD_STATE);
	raw = g_malloc(rh.length+1);

	if (fread(raw, rh.length, 1, nd->file) != 1)
		return FALSE;

	raw[rh.length] = '\0';

	ret = network_state_decode(raw, rh.length, NULL);

	g_free(raw);

	m1.data = &from_offset;
	m2.data = to_offset;
	linestack_replay(ctx, &m1, &m2, ret);

	/* Go back to original position */
	fseek(nd->file, save_offset, SEEK_SET);
	
	return ret;
}

static gboolean file_traverse(struct linestack_context *ctx,
		void *mf,
		void *mt,
		linestack_traverse_fn tf, 
		void *userdata)
{
	long *start_offset, *end_offset, save_offset;
	struct lf_data *nd = ctx->backend_data;

	if (nd == NULL) 
		return FALSE;

	save_offset = ftell(nd->file);

	/* Flush channel before reading otherwise data corruption may occur */
	fflush(nd->file);
	
	start_offset = mf;
	end_offset = mt;

	/* Go back to begin of file */
	fseek(nd->file, start_offset?*start_offset:0, SEEK_SET);
	
	while(!feof(nd->file) && (!end_offset || ftell(nd->file) <= *end_offset)) 
	{
		struct record_header rh;
		char *raw;
		struct line *l;
		
		if (fread(&rh, sizeof(rh), 1, nd->file) != 1) {
			log_global(NULL, LOG_WARNING, "read() failed");
			return FALSE;
		}

		if (rh.type == RECORD_STATE) { /* Skip state records */
			fseek(nd->file, rh.length, SEEK_CUR);
			continue;
		}

		raw = g_malloc(rh.length+1);
		
		if (fgets(raw, rh.length, nd->file) == NULL) {
			log_global(NULL, LOG_WARNING, "fgets() failed");
			return FALSE;
		}

		raw[rh.length] = '\0';

		l = irc_parse_line(raw);
		tf(l, rh.time, userdata);
		free_line(l);
	}

	fseek(nd->file, save_offset, SEEK_SET);

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
