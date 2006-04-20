/* 
	ctrlproxy: A modular IRC proxy
	(c) 2006 Jelmer Vernooij <jelmer@nl.linux.org>

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
#include <stdio.h>
#include <fcntl.h>
#include <sqlite.h>

#define STATE_DUMP_INTERVAL 1000

static gboolean sqlite_init(struct linestack_context *ctx, struct ctrlproxy_config *config)
{
	int rc;
	sqlite *db;
	char *fname, *err;

	fname = g_build_filename(config->config_dir, "linestack_sqlite", NULL);

	db = sqlite_open(fname, O_RDWR, &err);
	if (db == NULL) {
		log_global(NULL, LOG_WARNING, "Error opening linestack database '%s': %s", fname, err);
		return FALSE;
	}

	rc = sqlite_exec(db, "CREATE TABLE line (network text, id int primary key autoincrement, time int, data text)", NULL, NULL, &err);
	if (rc != SQLITE_OK) {
		log_global(NULL, LOG_WARNING, "Error creating table: %s", err);
		return FALSE;
	}

	rc = sqlite_exec(db, "CREATE TABLE state_dump (network text, data text)", NULL, NULL, &err);
	if (rc != SQLITE_OK) {
		log_global(NULL, LOG_WARNING, "Error creating table: %s", err);
		return FALSE;
	}

	ctx->backend_data = db;

	return TRUE;
}

static gboolean sqlite_fini(struct linestack_context *ctx)
{
	sqlite *db = ctx->backend_data;
	sqlite_close(db);
	return TRUE;
}

static gboolean sqlite_insert_line(struct linestack_context *ctx, const struct network *n, const struct line *l)
{
	sqlite *db = ctx->backend_data;
	int rc;
	char *err, *raw;
	time_t now = time(NULL);

	raw = irc_line_string(l);

	rc = sqlite_exec_printf(db, 
			"INSERT INTO line (network, time, data) VALUES ('%s',%d,'%s')",
			NULL, NULL, &err, n->name, now, raw);
	g_free(raw);
	if (rc != SQLITE_OK) {
		log_global(NULL, LOG_WARNING, "Error inserting data: %s", err);
		return FALSE;
	}

	return TRUE;
}

static void *sqlite_get_marker(struct linestack_context *ctx, struct network *n)
{
	sqlite *db = ctx->backend_data;

	/* FIXME */

	return NULL;
}

static 	struct network_state * sqlite_get_state (
		struct linestack_context *ctx, 
		struct network *n, 
		void *m)
{
	sqlite *db = ctx->backend_data;
	/* FIXME */
	return NULL;
}

static gboolean sqlite_traverse(struct linestack_context *ctx,
		struct network *n, 
		void *mf,
		void *mt,
		linestack_traverse_fn tf, 
		void *userdata)
{
	sqlite *db = ctx->backend_data;
	/* FIXME */

	return TRUE;
}

const struct linestack_ops linestack_file = {
	.name = "file",
	.init = sqlite_init,
	.fini = sqlite_fini,
	.insert_line = sqlite_insert_line,
	.get_marker = sqlite_get_marker,
	.get_state = sqlite_get_state, 
	.free_marker = g_free,
	.traverse = sqlite_traverse
};
