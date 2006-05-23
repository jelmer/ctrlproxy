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

struct linestack_sqlite_network {
	int last_state_id;
	int last_state_line_id;
	int last_line_id;
};

struct linestack_sqlite_data {
	GHashTable *networks;
	sqlite3 *db;
};

static gboolean insert_state_data(struct linestack_sqlite_data *data, const struct network *n);

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
	"nick text,"
	"fullname text,"
	"query int,"
	"modes text,"
	"hostmask text,"
	"state_id int"
	")",

	"CREATE TABLE IF NOT EXISTS channel_nick (nick text, channel text, mode text)",

	NULL
};

static struct linestack_sqlite_network *get_network_data(struct linestack_sqlite_data *data, const struct network *n)
{
	struct linestack_sqlite_network *ret = g_hash_table_lookup(data->networks, n);

	if (ret) 
		return ret;

	ret = g_new0(struct linestack_sqlite_network, 1);

	g_hash_table_insert(data->networks, n, ret);

	insert_state_data(data, n);

	return ret;
}

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

	data->networks = g_hash_table_new(g_str_hash, g_str_equal);

	ctx->backend_data = data;

	return TRUE;
}

static gboolean sqlite_fini(struct linestack_context *ctx)
{
	struct linestack_sqlite_data *data = ctx->backend_data;
	sqlite3_close(data->db);
	return TRUE;
}

static struct network_state *get_network_state(struct linestack_sqlite_data *data, int state_id, int *line_id)
{
	char *query;
	int rc, i, j;
	char **ret, *err;
	int nrow, ncolumn;
	struct network_state *state;
	char *mynick;

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

	mynick = g_strdup(ret[2]);
	*line_id = atoi(ret[3]);
	sqlite3_free_table(ret);
	
	state = g_new0(struct network_state, 1);

	query = sqlite3_mprintf("SELECT nick, fullname, query, modes, hostmask FROM network_nick WHERE state_id = %d", state_id);
	rc = sqlite3_get_table(data->db, query, &ret, &nrow, &ncolumn, &err);
	sqlite3_free(query);

	if (rc != SQLITE_OK) {
		log_global(NULL, LOG_WARNING, "Error network nicks: %s", err);
		g_free(state);
		g_free(mynick);
		return NULL;
	}

	if (nrow == 0) {
		log_global(NULL, LOG_WARNING, "Error: no network nicks returned");
		sqlite3_free_table(ret);
		g_free(state);
		g_free(mynick);
		return NULL;
	}

	for (i = 1; i <= nrow; i++) {
		struct network_nick *nn;
		
		if (!strcmp(mynick, ret[5*i])) {
			nn = &state->me;
		} else
			nn = g_new0(struct network_nick, 1);

		nn->fullname = g_strdup(ret[5*i+1]);
		network_nick_set_hostmask(nn, ret[5*i+2]);

		g_free(nn->nick);
		nn->nick = g_strdup(ret[5*i]);

		/*nn->modes = NULL; /* FIXME: ret[i][3] */

		if (strcmp(mynick, ret[5*i]) != 0) {
			state->nicks = g_list_append(state->nicks, nn);
			nn->query = atoi(ret[5*i+4]);
		} else
			nn->query = 1;
	}

	g_assert(state->me.nick);
	sqlite3_free_table(ret);
	g_free(mynick);

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

	for (i = 1; i <= nrow; i++) {
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
		cs->network = state;

		state->channels = g_list_append(state->channels, cs);

		ret1 = get_table(data, &nrow1, &ncol1, "SELECT hostmask FROM invitelist_entry WHERE channel = %s", ret[9*i]);

		for (j = 1; j <= nrow1; j++) {
			cs->invitelist = g_list_append(cs->invitelist, g_strdup(ret1[j]));
		}

		sqlite3_free_table(ret1);

		ret1 = get_table(data, &nrow1, &ncol1, "SELECT hostmask FROM exceptlist_entry WHERE channel = %s", ret[9*i]);

		for (j = 1; j <= nrow1; j++) {
			cs->exceptlist = g_list_append(cs->exceptlist, g_strdup(ret1[j]));
		}

		sqlite3_free_table(ret1);

		ret1 = get_table(data, &nrow1, &ncol1, "SELECT time, hostmask, byhostmask FROM banlist_entry WHERE channel = %s", ret[9*i]);

		for (j = 1; j <= nrow1; j++) {
			struct banlist_entry *be = g_new0(struct banlist_entry, 1);

			be->hostmask = g_strdup(ret1[j*3+1]);
			be->by = g_strdup(ret1[j*3+2]);
			be->time_set = atol(ret1[j*3]);

			cs->banlist = g_list_append(cs->banlist, be);
		}

		sqlite3_free_table(ret1);

		ret1 = get_table(data, &nrow1, &ncol1, "SELECT nick, mode FROM channel_nick WHERE channel = '%q'", ret[9*i+1]);

		g_assert(nrow1 > 1);

		for (j = 1; j <= nrow1; j++) {
			struct channel_nick *cn;

			cn = find_add_channel_nick(cs, ret1[j*2]);
			g_assert(cn);

			cn->mode = ret1[j*2+1][0];
		}

		sqlite3_free_table(ret1);
	}

	sqlite3_free_table(ret);


	return state;
}

static gboolean insert_state_data(struct linestack_sqlite_data *data, const struct network *n)
{
	struct linestack_sqlite_network *nd = get_network_data(data, n);
	GList *gl, *gl1;

	log_network("sqlite", LOG_TRACE, n, "Inserting state");

	if (!run_query(data, "INSERT INTO network_state (name, nick, line_id) VALUES ('%q','%q',%d)", n->name, n->state->me.nick, nd->last_line_id))
		return FALSE;

	nd->last_state_id = sqlite3_last_insert_rowid(data->db);

	for (gl = n->state->nicks; gl; gl = gl->next) {
		struct network_nick *nn = gl->data;

		if (!run_query(data, "INSERT INTO network_nick (nick,state_id,query,fullname,modes,hostmask) VALUES ('%q',%d,%d,'%q','%q','%q')", nn->nick, nd->last_state_id, nn->query, nn->fullname, mode2string(nn->modes), nn->hostmask))
			return FALSE;
	}

	if (!run_query(data, "INSERT INTO network_nick (nick,state_id,query,fullname,modes,hostmask) VALUES ('%q',%d,%d,'%q','%q','%q')", n->state->me.nick, nd->last_state_id, n->state->me.query, n->state->me.fullname, mode2string(n->state->me.modes), n->state->me.hostmask))
			return FALSE;

	for (gl = n->state->channels; gl; gl = gl->next) {
		int channel_id;
		char *modestring;
		struct channel_state *cs = gl->data;
		modestring = mode2string(cs->modes);

		if (!run_query(data, "INSERT INTO channel_state (state_id, name, topic, namreply_started, banlist_started, invitelist_started, exceptlist_started, nicklimit, key, modes, mode) VALUES (%d,'%q','%q',%d,%d,%d,%d,%d,'%q','%q','%s')", nd->last_state_id, cs->name, cs->topic, cs->namreply_started, cs->banlist_started, cs->invitelist_started, cs->exceptlist_started, cs->limit, cs->key, modestring, cs->mode)) {
			g_free(modestring);
			return FALSE;
		}
		g_free(modestring);
		
		channel_id = sqlite3_last_insert_rowid(data->db);

		for (gl1 = cs->nicks; gl1; gl1 = gl1->next) {
			struct channel_nick *cn = gl1->data;

			if (!run_query(data, "INSERT INTO channel_nick (nick, channel, mode) VALUES ('%q','%q','%c')", cn->global_nick->nick, cn->channel->name, cn->mode))
				return FALSE;

		}

		for (gl1 = cs->banlist; gl1; gl1 = gl1->next) {
			struct banlist_entry *be = gl1->data;

			if (!run_query(data, "INSERT INTO banlist_entry (channel, hostmask, by_hostmask, time) VALUES (%d, '%q', '%q', %d)", channel_id, be->hostmask, be->by, be->time_set))
				return FALSE;
		}

		for (gl1 = cs->exceptlist; gl1; gl1 = gl1->next) {
			if (!run_query(data, "INSERT INTO exceptlist_entry (channel, hostmask) VALUES (%d, '%q')", channel_id, gl1->data))
				return FALSE;
		}

		for (gl1 = cs->invitelist; gl1; gl1 = gl1->next) {
			if (!run_query(data, "INSERT INTO invitelist_entry (channel, hostmask) VALUES (%d, '%q')", channel_id, gl1->data))
				return FALSE;
		}
	}

	nd->last_state_line_id = nd->last_line_id;

	return TRUE;
}

static gboolean sqlite_insert_line(struct linestack_context *ctx, const struct network *n, const struct line *l)
{
	struct linestack_sqlite_data *data = ctx->backend_data;
	struct linestack_sqlite_network *nd = get_network_data(data, n);
	char *raw;
	time_t now = time(NULL);

	g_assert(n);
	g_assert(data->db);

	if (nd->last_line_id > nd->last_state_line_id + STATE_DUMP_INTERVAL)
		insert_state_data(data, n);

	raw = irc_line_string(l);

	if (!run_query(data, "INSERT INTO line (state_id, network, time, data) VALUES (%d, '%q',%lu,'%q')", nd->last_state_id, n->name, now, raw)) {
		g_free(raw);
		return FALSE;
	}

	g_free(raw);

	nd->last_line_id = sqlite3_last_insert_rowid(data->db);

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

static struct network_state * sqlite_get_state (
		struct linestack_context *ctx, 
		struct network *n, 
		void *m)
{
	struct linestack_sqlite_data *backend_data = ctx->backend_data;
	int rc;
	char *err;
	int id;
	int state_id, line_id;
	struct network_state *state;
	struct linestack_marker m1, m2;
	char *query;
	char **ret;
	int ncol, nrow;
	struct linestack_sqlite_network *nd = get_network_data(backend_data, n);

	if (m != NULL) 
		id = *(int *)m;
	else 
		id = nd->last_line_id;

	query = sqlite3_mprintf("SELECT state_id FROM line WHERE ROWID=%d", id);

	/* Find line for marker */
	rc = sqlite3_get_table(backend_data->db, query, &ret, &nrow, &ncol, &err);
	sqlite3_free(query);

	if (rc != SQLITE_OK) {
		log_global(NULL, LOG_WARNING, "Error getting line related to row: %s", err);
		return NULL;
	}

	if (nrow != 1) {
		log_global("sqlite", LOG_WARNING, "Expected 1 row, got %d", nrow);
		return NULL;
	}

	state_id = atoi(ret[1]);

	sqlite3_free_table(ret);
	
	state = get_network_state(backend_data, state_id, &line_id);

	m1.ctx = m2.ctx = ctx;
	m1.data = &line_id;
	m2.data = &id;
	linestack_replay(ctx, n, &m1, &m2, state);

	return state;
}

struct traverse_data {
	linestack_traverse_fn fn;
	void *userdata;
};

static int traverse_fn(void *_ret, int n, char **names, char **values)
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
	struct linestack_sqlite_network *nd = get_network_data(backend_data, n);

	data.fn = tf;
	data.userdata = userdata;

	if (mf != NULL)
		from = *(int *)mf;
	else
		from = nd->last_line_id;

	if (mt != NULL)
		to = *(int *)mt;
	else
		to = nd->last_line_id;

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
