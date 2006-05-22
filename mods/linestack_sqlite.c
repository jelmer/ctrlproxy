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

#include "internals.h"
#include <stdio.h>
#include <fcntl.h>
#include <sqlite3.h>

#define STATE_DUMP_INTERVAL 1000

struct linestack_sqlite_data {
	int last_state_id;
	int last_state_line_id;
	int last_line_id;
	sqlite3 *db;
};

static const char *tables[] = {
	"CREATE TABLE IF NOT EXISTS line(network text, state_id int, time int, data text)",
	"CREATE TABLE IF NOT EXISTS network_state ("
		"name text,"
		"nick text,"
		"line_id int"
	")",

	"CREATE TABLE IF NOT EXISTS banlist_entry (channel int, time int, hostmask text, byhostmask text)",
	"CREATE TABLE IF NOT EXISTS invitelist_entry (channel int, hostmask text)",
	"CREATE TABLE IF NOT EXISTS exceptlist_entry (channel int, hostmask text)",
	"CREATE TABLE IF NOT EXISTS channel_state ("
		"name text,"
		"topic text,"
		"namreply_started int,"
		"banlist_started int,"
		"invitelist_started int,"
		"exceptlist_started int,"
		"nicklimit int,"
		"modes text,"
		"mode text,"
		"key text,"
		"state_id int"
	")",

	"CREATE TABLE IF NOT EXISTS network_nick ("
	"fullname text,"
	"query int,"
	"modes text,"
	"hostmask text,"
	"state_id int"
	")",

	"CREATE TABLE IF NOT EXISTS channel_nick (nick text, channel text, mode text)",

	NULL
};

static char **get_table(struct linestack_sqlite_data *data, int *nrow, int *ncol, const char *fmt, ...)
{
	char *query;
	int rc;
	va_list ap;
	char **ret;
	char *err;

	va_start(ap, fmt);
	query = sqlite3_vmprintf(fmt, ap);
	va_end(ap);

	rc = sqlite3_get_table(data->db, query, &ret, nrow, ncol, &err);
	sqlite3_free(query);

	if (rc != SQLITE_OK) {
		log_global(NULL, LOG_WARNING, "Error running query '%s' %s", query, err);
		return NULL;
	}

	return ret;
}

static gboolean run_query(struct linestack_sqlite_data *data, const char *fmt, ...)
{
	va_list ap;
	int rc;
	char *query;
	char *err;

	va_start(ap, fmt);
	query = sqlite3_vmprintf(fmt, ap);
	va_end(ap);

	rc = sqlite3_exec(data->db, query, NULL, NULL, &err);
	sqlite3_free(query);

	if (rc != SQLITE_OK) {
		log_global(NULL, LOG_WARNING, "Error running query '%s' %s", fmt, err);
		return FALSE;
	}

	return TRUE;
}


static gboolean sqlite_init(struct linestack_context *ctx, struct ctrlproxy_config *config)
{
	int rc;
	struct linestack_sqlite_data *data = g_new0(struct linestack_sqlite_data, 1);
	char *fname, *err;
	int i;

	fname = g_build_filename(config->config_dir, "linestack_sqlite", NULL);

	rc = sqlite3_open(fname, &data->db);
	if (rc != SQLITE_OK) {
		log_global(NULL, LOG_WARNING, "Error opening linestack database '%s': %s", fname, sqlite3_errmsg(data->db));
		return FALSE;
	}

	for (i = 0; tables[i]; i++) {
		rc = sqlite3_exec(data->db, tables[i], NULL, NULL, &err);
		if (rc != SQLITE_OK) {
			log_global(NULL, LOG_WARNING, "Error creating table: %s", err);
			return FALSE;
		}
	}

	ctx->backend_data = data;

	return TRUE;
}

static gboolean sqlite_fini(struct linestack_context *ctx)
{
	struct linestack_sqlite_data *data = ctx->backend_data;
	sqlite3_close(data->db);
	return TRUE;
}

static struct network_state *get_network_state(struct linestack_sqlite_data *data, int state_id)
{
	char *query;
	int rc, i, j;
	char **ret, *err;
	int nrow, ncolumn;
	struct network_state *state;

	query = sqlite3_mprintf("SELECT nick, line_id FROM network_state WHERE ROWID = %d", state_id);

	rc = sqlite3_get_table(data->db, query, &ret, &nrow, &ncolumn, &err);
	sqlite3_free(query);

	if (rc != SQLITE_OK) {
		log_global(NULL, LOG_WARNING, "Error state data: %s", err);
		return NULL;
	}

	if (nrow == 0) {
		log_global(NULL, LOG_WARNING, "State data not found: %s", err);
		sqlite3_free_table(ret);
		return NULL;
	}
	
	state = g_new0(struct network_state, 1);

	/* FIXME */

	query = sqlite3_mprintf("SELECT fullname, query, modes, hostmask FROM network_nick WHERE state_id = %d", state_id);
	rc = sqlite3_get_table(data->db, query, &ret, &nrow, &ncolumn, &err);
	sqlite3_free(query);

	if (rc != SQLITE_OK) {
		log_global(NULL, LOG_WARNING, "Error network nicks: %s", err);
		g_free(state);
		return NULL;
	}

	if (nrow == 0) {
		log_global(NULL, LOG_WARNING, "Error: no network nicks returned");
		sqlite3_free_table(ret);
		g_free(state);
		return NULL;
	}

	for (i = 0; i < nrow; i++) {
		struct network_nick *nn = g_new0(struct network_nick, 1);

		nn->fullname = g_strdup(ret[4*i]);
		network_nick_set_hostmask(nn, ret[4*i+1]);
		/*nn->modes = NULL; /* FIXME: ret[i][2] */
		nn->query = atoi(ret[4*i+3]);

		state->nicks = g_list_append(state->nicks, nn);
	}

	sqlite3_free_table(ret);

	query = sqlite3_mprintf("SELECT ROWID, name, topic, namreply_started, banlist_started, invitelist_started, exceptlist_started, nicklimit, key FROM channel_state WHERE state_id = %d", state_id);
	rc = sqlite3_get_table(data->db, query, &ret, &nrow, &ncolumn, &err);
	sqlite3_free(query);

	if (rc != SQLITE_OK) {
		log_global(NULL, LOG_WARNING, "Error channel data: %s", err);
		g_free(state);
		return NULL;
	}

	if (nrow == 0) {
		log_global(NULL, LOG_WARNING, "Error channel data: %s", err);
		sqlite3_free_table(ret);
		g_free(state);
		return NULL;
	}

	for (i = 0; i < nrow; i++) {
		char **ret1;
		int nrow1, ncol1;
		struct channel_state *cs = g_new0(struct channel_state, 1);
		
		cs->name = g_strdup(ret[9*i+1]);
		cs->topic = g_strdup(ret[9*i+2]);
		cs->namreply_started = atoi(ret[9*i+3]);
		cs->banlist_started = atoi(ret[9*i+4]);
		cs->invitelist_started = atoi(ret[9*i+5]);
		cs->exceptlist_started = atoi(ret[9*i+6]);
		cs->limit = atoi(ret[9*i+7]);
		cs->key = g_strdup(ret[9*i+8]);

		ret1 = get_table(data, &nrow1, &ncol1, "SELECT * FROM invitelist_entry WHERE channel = %s", ret[9*i]);

		for (j = 0; j < nrow1; j++) {
			/* FIXME */
		}

		sqlite3_free_table(ret1);

		ret1 = get_table(data, &nrow1, &ncol1, "SELECT * FROM exceptlist_entry WHERE channel = %s", ret[9*i]);

		for (j = 0; j < nrow1; j++) {
			/* FIXME */
		}

		sqlite3_free_table(ret1);

		ret1 = get_table(data, &nrow1, &ncol1, "SELECT * FROM exceptlist_entry WHERE channel = %s", ret[9*i]);

		for (j = 0; j < nrow1; j++) {
			/* FIXME */
		}

		sqlite3_free_table(ret1);


		ret1 = get_table(data, &nrow1, &ncol1, "SELECT * FROM banlist_entry WHERE channel = %s", ret[9*i]);

		for (j = 0; j < nrow1; j++) {
			/* FIXME */
		}

		sqlite3_free_table(ret1);

		ret1 = get_table(data, &nrow1, &ncol1, "SELECT * FROM channel_nick WHERE channel = %s", ret[9*i]);

		for (j = 0; j < nrow1; j++) {
			/* FIXME */
		}

		sqlite3_free_table(ret1);

		state->channels = g_list_append(state->channels, cs);
	}

	sqlite3_free_table(ret);


	return state;
}

static gboolean insert_state_data(struct linestack_sqlite_data *data, const struct network *n)
{
	GList *gl, *gl1;

	log_network("sqlite", LOG_TRACE, n, "Inserting state");

	if (!run_query(data, "INSERT INTO network_state (name, nick, line_id) VALUES ('%q','%q',%d)", n->name, n->state->me.nick, data->last_line_id))
		return FALSE;

	data->last_state_id = sqlite3_last_insert_rowid(data->db);

	for (gl = n->state->nicks; gl; gl = gl->next) {
		struct network_nick *nn = gl->data;

		if (!run_query(data, "INSERT INTO network_nick (state_id,query,fullname,modes,hostmask) VALUES (%d,%d,'%q','%q','%q')", data->last_state_id, nn->query, nn->fullname, mode2string(nn->modes), nn->hostmask))
			return FALSE;
	}

	for (gl = n->state->channels; gl; gl = gl->next) {
		char *modestring;
		struct channel_state *cs = gl->data;
		modestring = mode2string(cs->modes);

		if (!run_query(data, "INSERT INTO channel_state (state_id, name, topic, namreply_started, banlist_started, invitelist_started, exceptlist_started, nicklimit, key, modes, mode) VALUES (%d,'%q','%q',%d,%d,%d,%d,%d,'%q','%q','%s')", data->last_state_id, cs->name, cs->topic, cs->namreply_started, cs->banlist_started, cs->invitelist_started, cs->exceptlist_started, cs->limit, cs->key, modestring, cs->mode)) {
			g_free(modestring);
			return FALSE;
		}
		g_free(modestring);

		for (gl1 = cs->nicks; gl1; gl1 = gl1->next) {
			struct channel_nick *cn = gl1->data;

			if (!run_query(data, "INSERT INTO channel_nick (nick, channel, mode) VALUES ('%q','%q','%c')", cn->global_nick->nick, cn->channel->name, cn->mode))
				return FALSE;

		}
		/* FIXME: insert invitelist, exceptlist, banlist */
	}

	data->last_state_line_id = data->last_line_id;

	return TRUE;
}

static gboolean sqlite_insert_line(struct linestack_context *ctx, const struct network *n, const struct line *l)
{
	struct linestack_sqlite_data *data = ctx->backend_data;
	char *raw;
	time_t now = time(NULL);

	g_assert(n);
	g_assert(data->db);

	if (data->last_line_id > data->last_state_line_id + STATE_DUMP_INTERVAL ||
		data->last_line_id == 0) {
		insert_state_data(data, n);
	}

	raw = irc_line_string(l);

	if (!run_query(data, "INSERT INTO line (state_id, network, time, data) VALUES (%d, '%q',%lu,'%q')", data->last_state_id, n->name, now, raw)) {
		g_free(raw);
		return FALSE;
	}

	g_free(raw);

	data->last_line_id = sqlite3_last_insert_rowid(data->db);

	return TRUE;
}

static void *sqlite_get_marker(struct linestack_context *ctx, struct network *n)
{
	struct linestack_sqlite_data *data = ctx->backend_data;
	char *err;
	int rc;
	char *query;
	char **ret;
	int nrow, ncol;
	int *result = g_new0(int, 1);

	query = sqlite3_mprintf("SELECT ROWID FROM line WHERE network='%q' ORDER BY ROWID DESC LIMIT 1", n->name);

	rc = sqlite3_get_table(data->db, query, &ret, &nrow, &ncol, &err);
	sqlite3_free(query);

	if (rc != SQLITE_OK) {
		log_global(NULL, LOG_WARNING, "Get marker: %s", err);
		return NULL;
	}

	*result = atoi(ret[0]);

	sqlite3_free_table(ret);

	return result;
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
	sr->length = 20; /* FIXME */
	return 0;
}

static struct network_state * sqlite_get_state (
		struct linestack_context *ctx, 
		struct network *n, 
		void *m)
{
	struct linestack_sqlite_data *backend_data = ctx->backend_data;
	int rc;
	struct state_result data;
	char *err;
	int id;
	int state_id;
	struct network_state *state;
	struct linestack_marker m1, m2;
	char *query;
	char **ret;
	int ncol, nrow;

	if (m != NULL) 
		id = *(int *)m;
	else 
		id = backend_data->last_line_id;

	query = sqlite3_mprintf("SELECT state_id FROM line WHERE ROWID=%d", id);

	/* Find line for marker */
	rc = sqlite3_get_table(backend_data->db, query, &ret, &nrow, &ncol, &err);
	sqlite3_free(query);

	if (rc != SQLITE_OK) {
		log_global(NULL, LOG_WARNING, "Error getting line related to row: %s", err);
		return NULL;
	}

	state_id = atoi(ret[1]);

	sqlite3_free_table(ret);
	
	state = get_network_state(backend_data, state_id);

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
	struct linestack_sqlite_data *backend_data = ctx->backend_data;
	int rc;
	char *err;
	int from, to;
	struct traverse_data data;
	char *query;

	data.fn = tf;
	data.userdata = userdata;

	if (mf != NULL)
		from = *(int *)mf;
	else
		from = backend_data->last_line_id;

	if (mt != NULL)
		to = *(int *)mt;
	else
		to = backend_data->last_line_id;

	query = sqlite3_mprintf("SELECT time,data FROM line WHERE network='%q' AND ROWID >= %d AND ROWID <= %d", n->name, from, to);

	rc = sqlite3_exec(backend_data->db, query, traverse_fn, &data, &err);
	sqlite3_free(query);

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
