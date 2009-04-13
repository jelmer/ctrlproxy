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
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>

#include <glib/gstdio.h>
#include <sys/stat.h>
#include <inttypes.h>

#define README_CONTENTS  \
"This directory contains the history information that ctrlproxy uses\n" \
"when sending backlogs.\n" \
"\n" \
"It is safe to remove this data while ctrlproxy is not running and \n" \
"will not break your configuration. \n" \
"\n" \
"If you delete it while ctrlproxy is running, you will lose the ability \n" \
"to use any backlog functionality during the current session.\n"


/*
 * Marshall/unmarshall functions
 */

enum marshall_mode { MARSHALL_PULL = 0, MARSHALL_PUSH = 1 };

typedef gboolean (*marshall_fn_t) (struct irc_network_state *, const char *name, int level, enum marshall_mode, GIOChannel *ch, void **data);

#define marshall_new(m,t) if ((m) == MARSHALL_PULL) *(t) = g_malloc(sizeof(**t));

#define LF_CHECK_IO_STATUS(status)	if (status != G_IO_STATUS_NORMAL) { \
		log_global(LOG_ERROR, "Unable to write to linestack file: %s", error->message); \
		g_error_free(error); \
		return FALSE; \
	}

static const char tabs[10] = {'\t', '\t', '\t', '\t', '\t', 
			       '\t', '\t', '\t', '\t', '\t' };


static GIOStatus marshall_set (GIOChannel *t, int level, const char *name, const char *value, GError **error)
{
	GIOStatus status;
	status = g_io_channel_write_chars(t, tabs, level, NULL, error);
	if (status != G_IO_STATUS_NORMAL)
		return status;

	g_assert(!strchr(name, '\n'));
	g_assert(value == NULL || !strchr(value, '\n'));

	status = g_io_channel_write_chars(t, name, -1, NULL, error);
	if (status != G_IO_STATUS_NORMAL)
		return status;

	if (value) {
		status = g_io_channel_write_chars(t, ": ", -1, NULL, error);
		if (status != G_IO_STATUS_NORMAL)
			return status;

		status = g_io_channel_write_chars(t, value, -1, NULL, error);
		if (status != G_IO_STATUS_NORMAL)
			return status;
	}

	status = g_io_channel_write_chars(t, "\n", -1, NULL, error);
	if (status != G_IO_STATUS_NORMAL)
		return status;

	return G_IO_STATUS_NORMAL;
}

static gboolean marshall_get (GIOChannel *t, int level, const char *name, char **value)
{
	char *line;
	GIOStatus status;
	GError *error = NULL;
	gsize term;

	status = g_io_channel_read_line(t, &line, NULL, &term, &error);
	LF_CHECK_IO_STATUS(status);

	line[term] = '\0';

	g_assert(strncmp(line, tabs, level) == 0);
	g_assert(strncmp(line+level, name, strlen(name)) == 0); 

	if (line[level+strlen(name)] == ':') {
		*value = g_strdup(line+level+strlen(name)+2);
	} else {
		*value = NULL;
	}

	g_free(line);

	return TRUE;
}

static gboolean marshall_struct(GIOChannel *t, enum marshall_mode m, int level, const char *name)
{
	g_assert(name);
	if (m == MARSHALL_PUSH) {
		GIOStatus status;
		GError *error = NULL;
		status = marshall_set(t, level, name, NULL, &error);
		return (status == G_IO_STATUS_NORMAL);
	} else {
		char *v;
		gboolean ret = marshall_get(t, level, name, &v);
		if (!ret) return FALSE;
		if (v != NULL) return FALSE;
		return TRUE;
	}
}

static gboolean marshall_string (struct irc_network_state *nst, 
								 const char *name, int level, 
								 enum marshall_mode m, GIOChannel *t, char **d)
{
	if (m == MARSHALL_PUSH) {
		GIOStatus status;
		GError *error = NULL;
		status = marshall_set(t, level, name, *d, &error);
		return (status == G_IO_STATUS_NORMAL);
	} else {
		return marshall_get(t, level, name, d);
	}
}

static gboolean marshall_bool(struct irc_network_state *nst, const char *name, 
							  int level, enum marshall_mode m, GIOChannel *t, 
							  gboolean *n)
{
	if (m == MARSHALL_PUSH) {
		GError *error = NULL;
		GIOStatus status;
		if (*n) status = marshall_set(t, level, name, "true", &error);
		else status = marshall_set(t, level, name, "false", &error);
		return (status == G_IO_STATUS_NORMAL);
	} else {
		gboolean ret;
		char *val;
		ret = marshall_get(t, level, name, &val);
		if (!ret) return FALSE;

		if (!strcmp(val, "true")) { *n = TRUE; g_free(val); return TRUE; }
		if (!strcmp(val, "false")) { *n = FALSE; g_free(val); return TRUE; }
		g_free(val);
		return FALSE;
	}
}

static gboolean marshall_char(struct irc_network_state *nst, const char *name, int level, enum marshall_mode m, GIOChannel *t, char *n)
{
	if (m == MARSHALL_PUSH) {
		char d[2] = {*n, '\0' };
		GError *error = NULL;
		GIOStatus status;
		/* FIXME: Escape \n ? */
		status = marshall_set(t, level, name, d, &error);
		return (status == G_IO_STATUS_NORMAL);
	} else {
		char *d;
		gboolean ret = marshall_get(t, level, name, &d);
		if (!ret) return FALSE;

		*n = d[0];

		g_free(d);

		return TRUE;
	}
}

static gboolean marshall_int(struct irc_network_state *nst, const char *name, int level, enum marshall_mode m, GIOChannel *t, int *n)
{
	if (m == MARSHALL_PUSH) {
		char tmp[10];
		GError *error = NULL;
		GIOStatus status;
		g_snprintf(tmp, sizeof(tmp), "%d", *n);
		status = marshall_set(t, level, name, tmp, &error);
		return (status == G_IO_STATUS_NORMAL);
	} else {
		char *tmp;
		gboolean ret = marshall_get(t, level, name, &tmp);
		if (!ret) return FALSE;

		*n = atoi(tmp);

		g_free(tmp);

		return TRUE;
	}
}

static gboolean marshall_time(struct irc_network_state *nst, const char *name, int level, enum marshall_mode m, GIOChannel *t, time_t *n)
{
	if (m == MARSHALL_PUSH) {
		char tmp[30];
		GError *error = NULL;
		GIOStatus status;
		g_snprintf(tmp, sizeof(tmp), "%ld", *n);
		status = marshall_set(t, level, name, tmp, &error);
		return (status == G_IO_STATUS_NORMAL);
	} else {
		char *tmp;
		gboolean ret = marshall_get(t, level, name, &tmp);
		if (!ret) return FALSE;

		*n = strtol(tmp, NULL, 0);

		g_free(tmp);

		return TRUE;
	}
}


static gboolean marshall_modes(struct irc_network_state *nst, const char *name, int level, enum marshall_mode m, GIOChannel *t, irc_modes_t n)
{
	if (m == MARSHALL_PUSH) {
		GError *error = NULL;
		char *tmp = mode2string(n);
		GIOStatus status = marshall_set(t, level, name, tmp, &error);
		g_free(tmp);
		return status == G_IO_STATUS_NORMAL;
	} else {
		char *tmp;
		gboolean ret = marshall_get(t, level, name, &tmp);
		if (!ret) return FALSE;
		string2mode(tmp, n);
		g_free(tmp);
		return TRUE;
	}
}

static gboolean marshall_network_nick (struct irc_network_state *nst, const char *name, int level, enum marshall_mode m, GIOChannel *t, struct network_nick *n)
{
	gboolean ret = TRUE;
	marshall_struct(t, m, level, name);
	ret &= marshall_bool(nst, "query", level+1, m, t, &n->query);
	ret &= marshall_modes(nst, "modes", level+1, m, t, n->modes);
	ret &= marshall_string(nst, "nick", level+1, m, t, &n->nick);
	g_assert(n->nick);
	ret &= marshall_string(nst, "fullname", level+1, m, t, &n->fullname);
	ret &= marshall_string(nst, "username", level+1, m, t, &n->username);
	ret &= marshall_string(nst, "hostname", level+1, m, t, &n->hostname);
	ret &= marshall_string(nst, "hostmask", level+1, m, t, &n->hostmask);
	ret &= marshall_string(nst, "server", level+1, m, t, &n->server);
	if (m == MARSHALL_PULL)
		n->channel_nicks = NULL;
	return ret;
}


static gboolean marshall_network_nick_p (struct irc_network_state *nst, const char *name, int level, enum marshall_mode m, GIOChannel *t, struct network_nick **n)
{
	marshall_new(m, n);
	return marshall_network_nick(nst, name, level, m, t, *n);
}

struct ht_traverse_data 
{
	marshall_fn_t key_fn;
	marshall_fn_t val_fn;
	struct data_blob *data;
	struct irc_network_state *nst;
};

static gboolean marshall_GList (struct irc_network_state *nst, const char *name, int level, enum marshall_mode m, GIOChannel *t, GList **gl, marshall_fn_t marshall_fn)
{
	marshall_struct(t, m, level, name);
	if (m == MARSHALL_PULL) {
		int count;
		int i;

		marshall_int(nst, "count", level+1, m, t, &count);

		*gl = NULL;

		for (i = 0; i < count; i++) {
			void *v;
			char name[20];
			g_snprintf(name, sizeof(name), "[%d]", i);
			if (!marshall_fn(nst, name, level+1, m, t, &v))
				return FALSE;
			*gl = g_list_append(*gl, v);
		}
	} else {
		int count = g_list_length(*gl);
		int i = 0;
		GList *l;

		marshall_int(nst, "count", level+1, m, t, &count);

		for (l = *gl; l; l = l->next) {
			char name[20];
			g_snprintf(name, sizeof(name), "[%d]", i);
			if (!marshall_fn(nst, name, level+1, m, t, &l->data))
				return FALSE;
			i++;
		}
	}

	return TRUE;
}

static gboolean marshall_nicklist_entry (struct irc_network_state *nst, const char *name, int level, enum marshall_mode m, GIOChannel *t, struct nicklist_entry **d)
{
	gboolean ret = TRUE;
	marshall_new(m, d);
	marshall_struct(t, m, level, name);
	ret &= marshall_time(nst, "time", level+1, m, t, &(*d)->time_set);
	ret &= marshall_string(nst, "hostmask", level+1, m, t, &(*d)->hostmask);
	ret &= marshall_string(nst, "by", level+1, m, t, &(*d)->by);
	return ret;
}

static gboolean marshall_channel_state (struct irc_network_state *nst, 
										const char *name, int level, 
										enum marshall_mode m, GIOChannel *t, 
										struct irc_channel_state **c)
{
	int i;
	gboolean ret = TRUE;
	marshall_new(m, c);
	marshall_struct(t, m, level, name);

	ret &= marshall_char(nst, "mode", level+1, m, t, &((*c)->mode));
	ret &= marshall_modes(nst, "modes", level+1, m, t, (*c)->modes);
	ret &= marshall_bool(nst, "namreply_started", level+1, m, t, &(*c)->namreply_started);
	ret &= marshall_bool(nst, "banlist_started", level+1, m, t, &(*c)->banlist_started);
	ret &= marshall_bool(nst, "invitelist_started", level+1, m, t, &(*c)->invitelist_started);
	ret &= marshall_bool(nst, "exceptlist_started", level+1, m, t, &(*c)->exceptlist_started);
	ret &= marshall_string(nst, "name", level+1, m, t, &(*c)->name);
	ret &= marshall_string(nst, "topic", level+1, m, t, &(*c)->topic);
	ret &= marshall_string(nst, "topic_set_by", level+1, m, t, &(*c)->topic_set_by);
	ret &= marshall_time(nst, "topic_set_time", level+1, m, t, &(*c)->topic_set_time);
	for (i = 0; i < MAXMODES; i++) {
		char name[20];
		g_snprintf(name, sizeof(name), "mode_nicklist[%d]", i);
		ret &= marshall_GList(nst, name, level+1, m, t, &(*c)->chanmode_nicklist[i], (marshall_fn_t)marshall_nicklist_entry);
		g_snprintf(name, sizeof(name), "mode_option[%d]", i);
		ret &= marshall_string(nst, name, level+1, m, t, &(*c)->chanmode_option[i]);
	}
	(*c)->network = nst;

	marshall_struct(t, m, level+1, "nicks");

	/* Nicks  */
	if (m == MARSHALL_PULL) {
		int count;
		int i;

		(*c)->nicks = NULL;

		marshall_int(nst, "count", level+2, m, t, &count);

		for (i = 0; i < count; i++) {
			struct channel_nick *cn;
			char *nick;
			char tmp[10];
			g_snprintf(tmp, sizeof(tmp), "[%d]", i);

			marshall_struct(t, m, level+2, tmp);
			
			if (!marshall_string(nst, "nick", level+3, m, t, &nick))
				return FALSE;
			
			cn = find_add_channel_nick(*c, nick);
			g_assert(cn);

			if (!marshall_modes(nst, "modes", level+3, m, t, cn->modes))
				return FALSE;

			g_free(nick);
		}
	} else {
		int count = g_list_length((*c)->nicks);
		char tmp[10];
		int i = 0;
		GList *l;

		marshall_int(nst, "count", level+2, m, t, &count);

		for (l = (*c)->nicks; l; l = l->next) {
			struct channel_nick *cn = l->data;
			g_snprintf(tmp, sizeof(tmp), "[%d]", i);

			marshall_struct(t, m, level+2, tmp);

			if (!marshall_string(nst, "nick", level+3, m, t, &cn->global_nick->nick))
				return FALSE;

			if (!marshall_modes(nst, "modes", level+3, m, t, cn->modes))
				return FALSE;

			i++;
		}
	}

	return ret;
}

static gboolean marshall_network_state(enum marshall_mode m, GIOChannel *t, 
									   struct irc_network_state *n)
{
	gboolean ret = TRUE;

	if (m == MARSHALL_PULL) {
		g_free(n->me.hostmask);
		n->me.hostmask = NULL;
	}
	ret &= marshall_network_nick(n, "me", 0, m, t, &n->me);
	ret &= marshall_GList(n, "nicks", 0, m, t, &n->nicks, (marshall_fn_t)marshall_network_nick_p);
	ret &= marshall_GList(n, "channels", 0, m, t, &n->channels, (marshall_fn_t)marshall_channel_state);

	g_assert(n->me.nick);

	return ret;
}

#define STATE_DUMP_INTERVAL 1000

/**
 * Linestack_file backend data
 */
struct lf_data {
	GIOChannel *line_file;
	char *state_dir;
	GIOChannel *state_file;
	struct {
		char *state_id;
		gint64 line_offset;
	} *state_dumps;
	size_t num_state_dumps;
	size_t num_state_dumps_alloc;
	int lines_since_last_state;
};

static char *state_path(struct lf_data *lf_data, const char *state_id)
{
	return g_build_filename(lf_data->state_dir, state_id, NULL);
}

static gboolean file_insert_state(struct linestack_context *ctx, 
							  const struct irc_network_state *state, 
							  const char *state_id);

static char *global_init(struct ctrlproxy_config *config)
{
	char *parent_dir, *readme_file;;
	parent_dir = g_build_filename(config->config_dir, "linestack_file", NULL);
	if (g_file_test(parent_dir, G_FILE_TEST_IS_DIR))
		return parent_dir;
	g_mkdir(parent_dir, 0700);
	readme_file = g_build_filename(parent_dir, "README", NULL);
	if (!g_file_test(readme_file, G_FILE_TEST_EXISTS)) {
		g_file_set_contents(readme_file, README_CONTENTS, 
							-1, NULL);
	}
	g_free(readme_file);
	return parent_dir;
}

static gboolean file_init(struct linestack_context *ctx, const char *name, 
						  gboolean truncate,
						  struct ctrlproxy_config *config, 
						  const struct irc_network_state *state)
{
	struct lf_data *data = g_new0(struct lf_data, 1);
	char *parent_dir, *data_dir, *data_file;
	GError *error = NULL;
	const char *fname;
	GDir *dir;
	const char *mode;

	parent_dir = global_init(config);

	data_dir = g_build_filename(parent_dir, name, NULL);
	g_assert(data_dir != NULL);

	g_free(parent_dir);
	g_mkdir(data_dir, 0700);
	data_file = g_build_filename(data_dir, "lines", NULL);
	g_assert(data_file != NULL);

	unlink(data_file);

	if (truncate)
		mode = "w+";
	else
		mode = "a+";

	data->line_file = g_io_channel_new_file(data_file, mode, &error);
	if (data->line_file == NULL) {
		log_global(LOG_WARNING, "Error opening `%s': %s", 
						  data_file, error->message);
		g_error_free(error);
		g_free(data_file);
		return FALSE;
	}
	g_free(data_file);

	g_io_channel_set_encoding(data->line_file, NULL, NULL);

	/* Remove old data file */
	data_file = g_build_filename(data_dir, "state", NULL);
	unlink(data_file);
	g_free(data_file);

	data->state_dir = g_build_filename(data_dir, "states", NULL);
	g_free(data_dir);
	g_mkdir(data->state_dir, 0755);

	dir = g_dir_open(data->state_dir, 0, &error);
	if (dir == NULL) {
		log_global(LOG_WARNING, "Error opening directory `%s': %s", 
						  data->state_dir, error->message);
		g_error_free(error);
		return FALSE;
	}

	while ((fname = g_dir_read_name(dir))) {
		char *state_file_path = state_path(data, fname);
		g_unlink(state_file_path);
		g_free(state_file_path);
	}

	g_dir_close(dir);

	ctx->backend_data = data;
	return file_insert_state(ctx, state, "START");
}

static gboolean file_fini(struct linestack_context *ctx)
{
	struct lf_data *data = ctx->backend_data;
	int i;
	g_io_channel_unref(data->line_file);
	g_free(data->state_dir);
	for (i = 0; i < data->num_state_dumps; i++)
		g_free(data->state_dumps[i].state_id);
	g_free(data->state_dumps);
	g_free(data);
	return TRUE;
}

static gint64 g_io_channel_tell_position(GIOChannel *gio)
{
	int fd = g_io_channel_unix_get_fd(gio);
	g_io_channel_flush(gio, NULL);
	return lseek(fd, 0, SEEK_CUR);
}

static gboolean file_insert_state(struct linestack_context *ctx, 
							  const struct irc_network_state *state,
							  const char *state_id)
{
	struct lf_data *nd = ctx->backend_data;
	GError *error = NULL;
	GIOStatus status;
	GIOChannel *state_file;
	char *data_file;

	log_global(LOG_TRACE, "Inserting state");

	data_file = state_path(nd, state_id);

	state_file = g_io_channel_new_file(data_file, "w+", &error);
	if (state_file == NULL) {
		log_global(LOG_WARNING, "Error opening `%s': %s", 
						  data_file, error->message);
		g_error_free(error);
		g_free(data_file);
		return FALSE;
	}
	g_free(data_file);

	g_io_channel_set_encoding(state_file, NULL, NULL);
	
	nd->lines_since_last_state = 0;

	if (nd->num_state_dumps >= nd->num_state_dumps_alloc) {
		nd->num_state_dumps_alloc += 1000;
		nd->state_dumps = g_realloc(nd->state_dumps, nd->num_state_dumps_alloc * sizeof(*nd->state_dumps));
	}

	nd->state_dumps[nd->num_state_dumps].line_offset = g_io_channel_tell_position(nd->line_file);
	nd->state_dumps[nd->num_state_dumps].state_id = g_strdup(state_id);

	nd->num_state_dumps++;

	marshall_network_state(MARSHALL_PUSH, state_file, (struct irc_network_state *)state);

	status = g_io_channel_flush(state_file, &error);
	LF_CHECK_IO_STATUS(status);

	g_io_channel_unref(state_file);

	return TRUE;
}

static gboolean file_insert_line(struct linestack_context *ctx, 
								 const struct irc_line *l, 
								 const struct irc_network_state *state)
{
	struct lf_data *nd = ctx->backend_data;
	char t[20];
	GError *error = NULL;
	GIOStatus status;
	gboolean ret;
	
	if (nd == NULL) 
		return FALSE;

	nd->lines_since_last_state++;
	if (nd->lines_since_last_state == STATE_DUMP_INTERVAL) {
		char *state_id = g_strdup_printf("%lu-%"PRIi64, time(NULL), 
								 g_io_channel_tell_position(nd->line_file));
		ret = file_insert_state(ctx, state, state_id);
		g_free(state_id);
		if (ret == FALSE)
			return FALSE;
	}

	g_snprintf(t, sizeof(t), "%ld ", time(NULL));

	status = g_io_channel_write_chars(nd->line_file, t, strlen(t), NULL, &error);
	LF_CHECK_IO_STATUS(status);

	status = irc_send_line(nd->line_file, (GIConv)-1, l, &error);

	if (status != G_IO_STATUS_NORMAL) {
		log_global(LOG_ERROR, "Unable to write to linestack file: %s", error->message);
		g_error_free(error);
		return FALSE;
	}

	return TRUE;
}

static void *file_get_marker(struct linestack_context *ctx)
{
	gint64 *pos;
	struct lf_data *nd = ctx->backend_data;

	pos = g_new0(gint64, 1);
	(*pos) = g_io_channel_tell_position(nd->line_file);
	return pos;
}

static struct irc_network_state *file_get_state (struct linestack_context *ctx, 
											  void *m)
{
	struct lf_data *nd = ctx->backend_data;
	struct irc_network_state *ret;
	gint64 *to_offset = m, t;
	struct linestack_marker m1, m2;
	GError *error = NULL;
	GIOStatus status;
	char *data_file;
	GIOChannel *state_file;
	int i;

	if (nd == NULL) 
		return NULL;

	/* Flush channel before reading otherwise data corruption may occur */
	status = g_io_channel_flush(nd->line_file, &error);
	LF_CHECK_IO_STATUS(status);

	if (to_offset != NULL) 
		t = *to_offset;
	else 
		t = g_io_channel_tell_position(nd->line_file);

	/* Search back fromend of the state file to begin 
	 * and find the state dump with the highest offset but an offset 
	 * below from_offset */
	for (i = nd->num_state_dumps-1; i >= 0; i--) {
		if (nd->state_dumps[i].line_offset <= t)
			break;
	}

	data_file = state_path(nd, nd->state_dumps[i].state_id);

	state_file = g_io_channel_new_file(data_file, "r", &error);
	if (state_file == NULL) {
		log_global(LOG_WARNING, "Error opening `%s': %s", 
						  data_file, error->message);
		g_error_free(error);
		g_free(data_file);
		return FALSE;
	}
	g_free(data_file);

	g_io_channel_set_encoding(state_file, NULL, NULL);

	ret = network_state_init("", "", "");
	if (!marshall_network_state(MARSHALL_PULL, state_file, ret))
		return NULL;

	m1.free_fn = NULL;
	m1.data = &nd->state_dumps[i].line_offset;
	m2.free_fn = NULL;
	m2.data = to_offset;
	linestack_replay(ctx, &m1, &m2, ret);

	g_io_channel_unref(state_file);
	
	return ret;
}

static gboolean file_traverse(struct linestack_context *ctx, void *mf,
		void *mt, linestack_traverse_fn tf, void *userdata)
{
	gint64 *start_offset, *end_offset;
	gboolean ret = TRUE;
	struct lf_data *nd = ctx->backend_data;
	GError *error = NULL;
	GIOStatus status = G_IO_STATUS_NORMAL;
	char *raw, *space;
	struct irc_line *l;

	if (nd == NULL) 
		return FALSE;

	/* Flush channel before reading otherwise data corruption may occur */
	g_io_channel_flush(nd->line_file, &error);
	
	start_offset = mf;
	end_offset = mt;

	/* Go back to begin of file */
	status = g_io_channel_seek_position(nd->line_file, 
			(start_offset != NULL)?(*start_offset):0, G_SEEK_SET, &error);

	LF_CHECK_IO_STATUS(status);

	raw = NULL;
	
	while((status != G_IO_STATUS_EOF) && 
		(end_offset == NULL || 
		 g_io_channel_tell_position(nd->line_file) <= (*end_offset))) {
		status = g_io_channel_read_line(nd->line_file, &raw, NULL, NULL, &error);
		if (status == G_IO_STATUS_ERROR) {
			log_global(LOG_WARNING, "read_line() failed: %s", error->message);
			g_error_free(error);
			return FALSE;
		}

		if (raw == NULL)
			break;

		space = strchr(raw, ' ');
		g_assert(space);
		*space = '\0';

		g_assert(space);
	
		l = irc_parse_line(space+1);
		ret &= tf(l, atol(raw), userdata);
		free_line(l);

		g_free(raw);
		raw = NULL;

		if (ret == FALSE) 
			break;
	}

	status = g_io_channel_seek_position(nd->line_file, 0, G_SEEK_END, &error);

	LF_CHECK_IO_STATUS(status);

	return ret;
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
