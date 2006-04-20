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
#include <sqlite3.h>

/* TODO: Insert state data */

#define STATE_DUMP_INTERVAL 1000

static gboolean sqlite_init(struct linestack_context *ctx, struct ctrlproxy_config *config)
{
	int rc;
	sqlite3 *db;
	char *fname, *err;

	fname = g_build_filename(config->config_dir, "linestack_sqlite", NULL);

	rc = sqlite3_open(fname, &db);
	if (rc != SQLITE_OK) {
		log_global(NULL, LOG_WARNING, "Error opening linestack database '%s': %s", fname, sqlite3_errmsg(db));
		return FALSE;
	}

	rc = sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS line(network text, state_id int, id int primary key, time int, data text)", NULL, NULL, &err);
	if (rc != SQLITE_OK) {
		log_global(NULL, LOG_WARNING, "Error creating table: %s", err);
		return FALSE;
	}

	rc = sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS state_dump(network text, data text, line_id int)", NULL, NULL, &err);
	if (rc != SQLITE_OK) {
		log_global(NULL, LOG_WARNING, "Error creating table: %s", err);
		return FALSE;
	}

	ctx->backend_data = db;

	return TRUE;
}

static gboolean sqlite_fini(struct linestack_context *ctx)
{
	sqlite3 *db = ctx->backend_data;
	sqlite3_close(db);
	return TRUE;
}

static gboolean sqlite_insert_line(struct linestack_context *ctx, const struct network *n, const struct line *l)
{
	sqlite3 *db = ctx->backend_data;
	int rc;
	char *err, *raw;
	time_t now = time(NULL);
	char *query;

	g_assert(n);
	g_assert(db);

	raw = irc_line_string(l);

	query = sqlite3_mprintf("INSERT INTO line (network, time, data) VALUES ('%s',%lu,'%s')", n->name, now, raw);
	rc = sqlite3_exec(db, query, NULL, NULL, &err);
	sqlite3_free(query);
	g_free(raw);
	if (rc != SQLITE_OK) {
		log_global(NULL, LOG_WARNING, "Error inserting data: %s", err);
		return FALSE;
	}

	return TRUE;
}

static int find_rowid (void *_ret,int n,char**names, char**values)
{
	int *ret = _ret;

	*ret = atoi(values[0]);

	return 0;
}

static void *sqlite_get_marker(struct linestack_context *ctx, struct network *n)
{
	sqlite3 *db = ctx->backend_data;
	char *err;
	int rc;
	int *ret = g_new0(int, 1);
	char *query;

	query = sqlite3_mprintf("SELECT ROWID FROM line WHERE network='%s' ORDER BY ROWID DESC LIMIT 1", n->name);

	rc = sqlite3_exec(db, query, find_rowid, ret, &err);
	sqlite3_free(query);

	if (rc != SQLITE_OK) {
		log_global(NULL, LOG_WARNING, "Get marker: %s", err);
		return NULL;
	}

	return ret;
}

static int find_stateid (void *_ret,int n,char**names, char**values)
{
	int *ret = _ret;

	*ret = atoi(values[0]);

	return 0;
}

struct state_result {
	char *data;
	int length;
	int line_id;
};

static int get_state_data(void *_ret,int n,char**names, char**values)
{
	struct state_result *sr = _ret;

	sr->line_id = atoi(values[0]);
	sr->data = g_strdup(values[1]);
	sr->length = sqlite3_decode_binary(sr->data, sr->data);
	return 0;
}

static struct network_state * sqlite_get_state (
		struct linestack_context *ctx, 
		struct network *n, 
		void *m)
{
	sqlite3 *db = ctx->backend_data;
	int rc;
	struct state_result data;
	char *err;
	int id = *(int *)m;
	int state_id;
	struct network_state *state;
	struct linestack_marker m1, m2;
	char *query;

	query = sqlite3_mprintf("SELECT state_id FROM line WHERE ROWID=%d", id);

	/* Find line for marker */
	rc = sqlite3_exec(db, query, find_stateid, &state_id, &err);
	sqlite3_free(query);

	if (rc != SQLITE_OK) {
		log_global(NULL, LOG_WARNING, "Error getting line related to row: %s", err);
		return NULL;
	}

	/* Get state */
	query = sqlite3_mprintf("SELECT line_id,data FROM state_dump WHERE ROWID=%d AND network='%s'", state_id, n->name);
	rc = sqlite3_exec(db, query, get_state_data, &data, &err);
	sqlite3_free(query);

	if (rc != SQLITE_OK) {
		log_global(NULL, LOG_WARNING, "Error getting state with id %d: %s", state_id, err);
		return NULL;
	}

	state = network_state_decode(data.data, data.length, &n->info);

	g_free(data.data);

	m1.ctx = m2.ctx = ctx;
	m1.data = &data.line_id;
	m2.data = &id;
	linestack_replay(ctx, n, &m1, &m2, state);

	return state;
}

struct traverse_data {
	linestack_traverse_fn fn;
	void *userdata;
};

static int traverse_fn(void *_ret,int n,char**names, char**values)
{
	struct traverse_data *data = _ret;
	struct line *l = irc_parse_line(values[1]);

	data->fn(l, atoi(values[0]), data->userdata);

	return 0;
}

static gboolean sqlite_traverse(struct linestack_context *ctx,
		struct network *n, 
		void *mf,
		void *mt,
		linestack_traverse_fn tf, 
		void *userdata)
{
	sqlite3 *db = ctx->backend_data;
	int rc;
	char *err;
	int from, to;
	struct traverse_data data;

	data.fn = tf;
	data.userdata = userdata;

	from = *(int *)mf;
	to = *(int *)mt;

	rc = sqlite_exec_printf(db, "SELECT time,data FROM line WHERE network='%s' AND ROWID >= %d AND ROWID <= %d", 
		traverse_fn, &data, &err, n->name, from, to);
	if (rc != SQLITE_OK) {
		log_global(NULL, LOG_WARNING, "Error traversing lines between %d and %dd: %s", from, to, err);
		return FALSE;
	}

	return TRUE;
}

const struct linestack_ops linestack_sqlite = {
	.name = "sqlite",
	.init = sqlite_init,
	.fini = sqlite_fini,
	.insert_line = sqlite_insert_line,
	.get_marker = sqlite_get_marker,
	.get_state = sqlite_get_state, 
	.free_marker = g_free,
	.traverse = sqlite_traverse
};

static gboolean init_plugin(void)
{
	register_linestack(&linestack_sqlite);
	return TRUE;
}

struct plugin_ops plugin = {
	.name = "linestack_sqlite",
	.version = 0,
	.init = init_plugin,
};
