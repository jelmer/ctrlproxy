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

/* Index file format
 * 8 bytes - offset of line
 * 8 bytes - current time
 * 8 bytes - index of the last line with state printed
 */

#define INDEX_RECORD_SIZE (sizeof(guint64) + sizeof(time_t) + sizeof(guint64))

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
		log_global(LOG_ERROR, "%s:%d: Unable to write to linestack file: %s", \
				   __FILE__, __LINE__, error != NULL?error->message:"Unknown"); \
		if (error != NULL) g_error_free(error); \
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
	GIOChannel *index_file;
	int count;
	int last_line_with_state;
};

static char *state_path(struct lf_data *lf_data, guint64 state_id)
{
	char *state_id_str;
	char *ret;
	state_id_str = g_strdup_printf("%"PRIi64, state_id);
	if (state_id_str == NULL)
		return NULL;
	ret = g_build_filename(lf_data->state_dir, state_id_str, NULL);
	g_free(state_id_str);
	return ret;
}

static gboolean file_insert_state(struct linestack_context *ctx, 
							  const struct irc_network_state *state, 
							  guint64 state_id);

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
	char *index_file, *state_file;
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

	index_file = g_build_filename(data_dir, "index", NULL);
	g_assert(index_file != NULL);

	data->index_file = g_io_channel_new_file(index_file, mode, &error);
	if (data->index_file == NULL) {
		log_global(LOG_WARNING, "Error opening `%s': %s", 
						  index_file, error->message);
		g_error_free(error);
		g_free(index_file);
		return FALSE;
	}
	g_free(index_file);

	g_io_channel_set_encoding(data->index_file, NULL, NULL);

	state_file = g_build_filename(data_dir, "state", NULL);
	unlink(state_file);
	g_free(state_file);

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
		char *state_file_path = g_build_filename(data->state_dir, fname, NULL);
		g_unlink(state_file_path);
		g_free(state_file_path);
	}

	g_dir_close(dir);

	ctx->backend_data = data;
	return file_insert_state(ctx, state, 0);
}

static gboolean file_fini(struct linestack_context *ctx)
{
	struct lf_data *data = ctx->backend_data;
	g_io_channel_unref(data->line_file);
	g_io_channel_unref(data->index_file);
	g_free(data->state_dir);
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
							  guint64 state_id)
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
	
	nd->last_line_with_state = nd->count;

	marshall_network_state(MARSHALL_PUSH, state_file, (struct irc_network_state *)state);

	status = g_io_channel_flush(state_file, &error);
	LF_CHECK_IO_STATUS(status);

	g_io_channel_unref(state_file);

	return TRUE;
}

gboolean write_index_entry(GIOChannel *ioc, guint64 line_offset, time_t time,
					   guint64 state_line_index)
{
	GError *error = NULL;
	GIOStatus status;

	status = g_io_channel_write_chars(ioc, (void *)&line_offset, sizeof(guint64), NULL, 
									  &error);
	LF_CHECK_IO_STATUS(status);

	status = g_io_channel_write_chars(ioc, (void *)&time, sizeof(time_t), NULL, &error);
	LF_CHECK_IO_STATUS(status);

	status = g_io_channel_write_chars(ioc, (void *)&state_line_index, sizeof(guint64),
									  NULL, &error);
	LF_CHECK_IO_STATUS(status);

	return TRUE;
}

static gboolean file_insert_line(struct linestack_context *ctx, 
								 const struct irc_line *l, 
								 const struct irc_network_state *state)
{
	struct lf_data *nd = ctx->backend_data;
	GError *error = NULL;
	GIOStatus status;
	gboolean ret;
	
	if (nd == NULL) 
		return FALSE;

	if (nd->count >= nd->last_line_with_state + STATE_DUMP_INTERVAL) {
		ret = file_insert_state(ctx, state, nd->count);
		if (ret == FALSE)
			return FALSE;
	}

	if (!write_index_entry(nd->index_file, 
					  g_io_channel_tell_position(nd->line_file),
					  time(NULL),
					  nd->last_line_with_state))
		return FALSE;

	status = irc_send_line(nd->line_file, (GIConv)-1, l, &error);

	if (status != G_IO_STATUS_NORMAL) {
		log_global(LOG_ERROR, "Unable to write to linestack file: %s", error->message);
		g_error_free(error);
		return FALSE;
	}

	nd->count++;
	return TRUE;
}

static void *file_get_marker(struct linestack_context *ctx)
{
	gint64 *pos;
	struct lf_data *nd = ctx->backend_data;

	pos = g_new0(gint64, 1);
	(*pos) = nd->count;
	return pos;
}

static struct irc_network_state *file_get_state (struct linestack_context *ctx, 
											  void *m)
{
	struct lf_data *nd = ctx->backend_data;
	struct irc_network_state *ret;
	gint64 *to_index = m, t, state_index;
	struct linestack_marker m1, m2;
	GError *error = NULL;
	GIOStatus status;
	char *data_file;
	GIOChannel *state_file;

	if (nd == NULL) 
		return NULL;

	/* Flush channel before reading otherwise data corruption may occur */
	status = g_io_channel_flush(nd->line_file, &error);
	LF_CHECK_IO_STATUS(status);

	status = g_io_channel_flush(nd->index_file, &error);
	LF_CHECK_IO_STATUS(status);

	if (to_index != NULL) {
		t = (*to_index)-1;
		status = g_io_channel_seek_position(nd->index_file, 
			t * INDEX_RECORD_SIZE + sizeof(guint64) + sizeof(time_t),
			G_SEEK_SET, &error);
		LF_CHECK_IO_STATUS(status);
		status = g_io_channel_read_chars(nd->index_file, (void *)&state_index, 
										 sizeof(guint64), NULL, &error);
		if (status == G_IO_STATUS_ERROR) {
			log_global(LOG_ERROR, "Unable to ready entry %"PRIi64" in index: %s",
					   t, error->message);
			g_error_free(error);
			return FALSE;
		} else if (status == G_IO_STATUS_EOF) {
			log_global(LOG_ERROR, "EOF reading entry %"PRIi64" in index", t);
			return FALSE;
		}

		/* Go back to the end of the index file */
		status = g_io_channel_seek_position(nd->index_file, 0, G_SEEK_END,
											&error);
		LF_CHECK_IO_STATUS(status);
	} else {
		state_index = nd->last_line_with_state;
	}

	data_file = state_path(nd, state_index);

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
	m1.data = &state_index;
	m2.free_fn = NULL;
	m2.data = to_index;
	linestack_replay(ctx, &m1, &m2, ret);

	g_io_channel_unref(state_file);
	
	return ret;
}

static gboolean file_traverse(struct linestack_context *ctx, void *mf,
		void *mt, linestack_traverse_fn tf, void *userdata)
{
	gint64 start_index, end_index;
	gboolean ret = TRUE;
	struct lf_data *nd = ctx->backend_data;
	GError *error = NULL;
	GIOStatus status = G_IO_STATUS_NORMAL;
	char *raw;
	struct irc_line *l;
	guint64 i;

	if (nd == NULL) 
		return FALSE;

	/* Flush channel before reading otherwise data corruption may occur */
	g_io_channel_flush(nd->line_file, &error);

	g_io_channel_flush(nd->index_file, &error);
	
	if (mf == NULL) {
		start_index = 0;
	} else {
		start_index = *((guint64 *)mf);
	}
	
	if (mt == NULL) {
		end_index = nd->count;
	} else {
		end_index = *((guint64 *)mt);
	}

	raw = NULL;

	for (i = start_index; i < end_index; i++) {
		guint64 offset;
		time_t time;

		status = g_io_channel_seek_position(nd->index_file, 
											i * INDEX_RECORD_SIZE, 
											G_SEEK_SET, &error);
		if (status != G_IO_STATUS_NORMAL) {
			log_global(LOG_WARNING, "seeking line %"PRIi64" in index failed: %s", i, 
					   error->message);
			g_error_free(error);
			ret = FALSE;
			goto cleanup;
		}

		status = g_io_channel_read_chars(nd->index_file, (void *)&offset, 
										 sizeof(guint64), NULL, &error);
		if (status != G_IO_STATUS_NORMAL) {
			log_global(LOG_WARNING, "reading line %"PRIi64" in index failed: %s", i, 
					   error->message);
			g_error_free(error);
			ret = FALSE;
			goto cleanup;
		}

		status = g_io_channel_read_chars(nd->index_file, (void *)&time, 
										 sizeof(time_t), NULL, &error);
		if (status != G_IO_STATUS_NORMAL) {
			log_global(LOG_WARNING, "reading time %"PRIi64" in index failed: %s", i, 
					   error->message);
			g_error_free(error);
			ret = FALSE;
			goto cleanup;
		}

		status = g_io_channel_seek_position(nd->line_file, offset, G_SEEK_SET, 
											&error);
		if (status != G_IO_STATUS_NORMAL) {
			log_global(LOG_WARNING, "seeking line %"PRIi64" (%"PRIi64") in data failed: %s",
					   i, offset, error->message);
			g_error_free(error);
			ret = FALSE;
			goto cleanup;
		}

		status = g_io_channel_read_line(nd->line_file, &raw, NULL, NULL, &error);
		if (status == G_IO_STATUS_ERROR) {
			log_global(LOG_WARNING, "read_line() failed: %s", error->message);
			g_error_free(error);
			ret = FALSE;
			goto cleanup;
		}

		l = irc_parse_line(raw);
		ret &= tf(l, time, userdata);
		free_line(l);

		g_free(raw);
		raw = NULL;

		if (ret == FALSE) 
			break;
	}

cleanup:
	status = g_io_channel_seek_position(nd->line_file, 0, G_SEEK_END, &error);
	LF_CHECK_IO_STATUS(status);

	status = g_io_channel_seek_position(nd->index_file, 0, G_SEEK_END, &error);
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
