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

#include "internals.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

static GSList *linestack_backends = NULL;

void register_linestack(const struct linestack_ops *b)
{
	linestack_backends = g_slist_append(linestack_backends, b);
}

struct linestack_context *new_linestack(struct ctrlproxy_config *cfg)
{
	extern const struct linestack_ops linestack_file;
	struct linestack_context *ctx;
	struct linestack_ops *current_backend = NULL;

	register_linestack(&linestack_file);

	if (cfg && cfg->linestack_backend) {
		GSList *gl;
		for (gl = linestack_backends; gl ; gl = gl->next) {
			struct linestack_ops *ops = gl->data;
			if (!strcmp(ops->name, cfg->linestack_backend)) {
				current_backend = ops;
				break;
			}
		}

		if (!current_backend) 
			log_global(NULL, LOG_WARNING, "Unable to find linestack backend %s: falling back to default", cfg->linestack_backend);
	}

	if (!current_backend) {
		current_backend = &linestack_file;
	}

	ctx = g_new0(struct linestack_context, 1);
	ctx->ops = current_backend;
	current_backend->init(ctx, cfg);

	return ctx;
}

void free_linestack_context(struct linestack_context *ctx)
{
	if (ctx)
		ctx->ops->fini(ctx);

	g_free(ctx);
}

static struct linestack_marker *wrap_linestack_marker(struct linestack_context *ctx, void *data)
{
	struct linestack_marker *mrk;
	if (data == NULL)
		return NULL;

	mrk = g_new0(struct linestack_marker, 1);
	mrk->ctx = ctx;
	mrk->data = data;
	return mrk;
}

struct linestack_marker *linestack_get_marker_numlines (struct linestack_context *ctx, struct network *n, int lines)
{
	if (!ctx->ops) return NULL;
	if (!ctx->ops->get_marker_numlines) return NULL;

	return wrap_linestack_marker(ctx, ctx->ops->get_marker_numlines(ctx, n, lines));
}

struct network_state *linestack_get_state(
		struct linestack_context *ctx,
		struct network *n, 
		struct linestack_marker *lm)
{
	struct network_state *st;
	if (!ctx->ops) return NULL;
	if (!ctx->ops->get_state) return NULL;

	st = ctx->ops->get_state(ctx, n, lm?lm->data:NULL);
	if (st == NULL)
		return NULL;

	g_assert(st->me.nick);
	g_assert(st->me.query);
	return st;
}

gboolean linestack_traverse(
		struct linestack_context *ctx,					
		struct network *n, 
		struct linestack_marker *lm_from,
		struct linestack_marker *lm_to,
		linestack_traverse_fn handler, 
		void *userdata)
{
	if (!ctx->ops) return FALSE;
	g_assert(ctx->ops->traverse);

	return ctx->ops->traverse(ctx, n, lm_from?lm_from->data:NULL, lm_to?lm_to->data:NULL, handler, userdata);
}

struct traverse_object_data {
	linestack_traverse_fn handler;
	void *userdata;
};

static void traverse_object_handler(struct line *l, time_t t, void *state)
{
	struct traverse_object_data *d = state;
	d->handler(l, t, d->userdata);
}

gboolean linestack_traverse_object(
			struct linestack_context *ctx,
			struct network *n,
			const char *obj, 
			struct linestack_marker *lm_from, 
			struct linestack_marker *lm_to, linestack_traverse_fn hl,
			void *userdata)
{
	struct traverse_object_data d;
	if (!ctx->ops) return FALSE;

	d.userdata = userdata;
	d.handler = hl;
	
	return linestack_traverse(ctx, n, lm_from?lm_from->data:NULL, lm_to?lm_to->data:NULL, traverse_object_handler, &d);
}

void linestack_free_marker(struct linestack_marker *lm)
{
	if (!lm) return;
	g_assert(lm->ctx->ops);
	if (!lm->ctx->ops->free_marker) return;
	lm->ctx->ops->free_marker(lm->data);
}

struct linestack_marker *linestack_get_marker(struct linestack_context *ctx, struct network *n)
{
	if (!ctx->ops) return NULL;
	g_assert(ctx->ops->get_marker);

	return wrap_linestack_marker(ctx, ctx->ops->get_marker(ctx, n));
}

static const char *linestack_messages[] = { 
	"NICK", "JOIN", "QUIT", "PART", "PRIVMSG", "NOTICE", "KICK", 
	"MODE", "TOPIC", NULL };

gboolean linestack_insert_line(struct linestack_context *ctx, const struct network *n, const struct line *l, enum data_direction dir)
{
	int i;
	gboolean needed = FALSE;

	if (l->argc == 0) return FALSE;

	if (!ctx->ops) return FALSE;
	g_assert(ctx->ops->insert_line);

	/* Only need PRIVMSG and NOTICE messages we send ourselves */
	if (dir == TO_SERVER && 
		g_strcasecmp(l->args[0], "PRIVMSG") && 
		g_strcasecmp(l->args[0], "NOTICE")) return FALSE;

	/* No CTCP, please */
	if ((!g_strcasecmp(l->args[0], "PRIVMSG") ||
		!g_strcasecmp(l->args[0], "NOTICE")) && 
		l->argc > 2 && l->args[2][0] == '\001')
		return FALSE;

	for (i = 0; linestack_messages[i]; i++) 
		if (!g_strcasecmp(linestack_messages[i], l->args[0]))
			needed = TRUE;

	if (!needed) return FALSE;

	return ctx->ops->insert_line(ctx, n, l);
}

static void send_line(struct line *l, time_t t, void *_client)
{
	struct client *c = _client;
	client_send_line(c, l);
}

static void send_line_timed(struct line *l, time_t t, void *_client)
{
	struct client *c = _client;

	if ((!g_strcasecmp(l->args[0], "PRIVMSG") ||
		!g_strcasecmp(l->args[0], "NOTICE")) &&
		l->argc > 2) {
		struct line *nl = linedup(l);
		char stime[512];
		char *tmp;

		strftime(stime, sizeof(stime), "%H:%M:%S", localtime(&t));
		tmp = g_strdup_printf("[%s] %s", stime, nl->args[2]);
		g_free(nl->args[2]);
		nl->args[2] = tmp;
		client_send_line(c, nl);
		free_line(nl);
	} else {
		client_send_line(c, l);
	}
}

gboolean linestack_send(struct linestack_context *ctx, struct network *n, struct linestack_marker *mf, struct linestack_marker *mt, const struct client *c)
{
	return linestack_traverse(ctx, n, mf, mt, send_line, c);
}

gboolean linestack_send_timed(struct linestack_context *ctx, struct network *n, struct linestack_marker *mf, struct linestack_marker *mt, const struct client *c)
{
	return linestack_traverse(ctx, n, mf, mt, send_line_timed, c);
}

gboolean linestack_send_object(struct linestack_context *ctx, struct network *n, const char *obj, struct linestack_marker *mf, struct linestack_marker *mt, const struct client *c)
{
	return linestack_traverse_object(ctx, n, obj, mf, mt, send_line, c);
}

gboolean linestack_send_object_timed(struct linestack_context *ctx, struct network *n, const char *obj, struct linestack_marker *mf, struct linestack_marker *mt, const struct client *c)
{
	return linestack_traverse_object(ctx, n, obj, mf, mt, send_line_timed, c);
}

static void replay_line(struct line *l, time_t t, void *state)
{
	struct network_state *st = state;
	state_handle_data(st, l);
}

gboolean linestack_replay(struct linestack_context *ctx, struct network *n, struct linestack_marker *mf, struct linestack_marker *mt, struct network_state *st)
{
	return linestack_traverse(ctx, n, mf, mt, replay_line, st);
}