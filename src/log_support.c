/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2007 Jelmer Vernooij <jelmer@nl.linux.org>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
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

#define MAX_OPEN_LOGFILES 300

static void free_file_info(void *_data)
{
	struct log_file_info *data = _data;

	fclose(data->file);
	g_free(data);
}

struct log_support_context *log_support_init(void)
{
	struct log_support_context *ret = g_new0(struct log_support_context, 1);

	ret->files = g_hash_table_new_full(g_str_hash, g_str_equal, 
			g_free, free_file_info);

	return ret;
}

void free_log_support_context(struct log_support_context *ret)
{
	g_hash_table_destroy(ret->files);
	g_free(ret);
}

#define CLEANUP_THRESHOLD (60 * 60)

static gboolean eval_remove(gpointer key, gpointer value, gpointer user_data)
{
	struct log_file_info *fi = value;
	struct log_support_context *ctx = user_data;

	if (fi->last_used < time(NULL) - CLEANUP_THRESHOLD) {
		ctx->num_opened--;
		return TRUE;
	}

	return FALSE;
}

void log_support_cleanup(struct log_support_context *ctx)
{
	g_hash_table_foreach_remove(ctx->files, eval_remove, ctx);
}

gboolean log_support_write(struct log_support_context *ctx, 
					   const char *path,
					   const char *text)
{
	struct log_file_info *fi;
	
	/* First, check if the file is still present in the hash table */
	fi = g_hash_table_lookup(ctx->files, path);

	if (fi == NULL) {
		fi = g_new0(struct log_file_info, 1);
		g_hash_table_insert(ctx->files, g_strdup(path), fi);
	}

	if (fi->file == NULL) {
		char *dirpath;
		if (ctx->num_opened > MAX_OPEN_LOGFILES)
			log_support_cleanup(ctx);

		dirpath = g_path_get_dirname(path);
		if (g_mkdir_with_parents(dirpath, 0700) == -1) {
			log_global(LOG_ERROR, "Unable to create directory `%s'!", dirpath);
			g_free(dirpath);
			return FALSE;
		}
		g_free(dirpath);

		/* Then open the correct filename */
		fi->file = fopen(path, "a+");
		if (fi->file == NULL) {
			log_global(LOG_ERROR, "Couldn't open file %s for logging!", path);
			return FALSE;
		}

		ctx->num_opened++;
	}

	fputs(text, fi->file);
	fflush(fi->file);

	fi->last_used = time(NULL);
	return TRUE;
}

void log_support_writef(struct log_support_context *ctx, 
					   const char *path,
					   const char *fmt, ...)
{
	va_list ap;
	char *ret;
	va_start(ap, fmt);
	ret = g_strdup_vprintf(fmt, ap);
	log_support_write(ctx, path, ret);
	va_end(ap);
}
