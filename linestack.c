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
static struct linestack_ops *current_backend = NULL;

void register_linestack(struct linestack_ops *b)
{
	linestack_backends = g_slist_append(linestack_backends, b);
}

void unregister_linestack(struct linestack_ops *b)
{
	linestack_backends = g_slist_remove(linestack_backends, b);
}

void init_linestack(struct ctrlproxy_config *cfg)
{
	if (cfg->linestack_backend) {
		GSList *gl;
		for (gl = linestack_backends; gl ; gl = gl->next) {
			struct linestack_ops *ops = gl->data;
			if (!strcmp(ops->name, cfg->linestack_backend)) {
				current_backend = ops;
				break;
			}
		}

		if (!current_backend) 
			log_global(NULL, "Unable to find linestack backend %s: falling back to default", cfg->linestack_backend);
	}

	if (!linestack_backends) return;

	if (!current_backend) {
		current_backend = linestack_backends->data;
	}

	current_backend->init();
}

void fini_linestack()
{
	if (current_backend)
		current_backend->fini();
}

linestack_marker *linestack_get_marker_numlines (struct network *n, int lines)
{
	if (!current_backend) return NULL;
	if (!current_backend->get_marker_numlines) return NULL;

	return current_backend->get_marker_numlines(n, lines);
}

struct network_state *linestack_get_state(struct network *n, 
		linestack_marker *lm)
{
	/* FIXME: Return current state rather then NULL in case of 
	 * failure ? */
	if (!current_backend) return NULL;
	if (!current_backend->get_state) return NULL;

	return current_backend->get_state(n, lm);
}

gboolean linestack_traverse(struct network *n, 
		linestack_marker *lm_from,
		linestack_marker *lm_to,
		linestack_traverse_fn handler, 
		void *userdata)
{
	if (!current_backend) return FALSE;
	g_assert(current_backend->traverse);

	return current_backend->traverse(n, lm_from, lm_to, handler, userdata);
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

gboolean linestack_traverse_object(struct network *n,
			const char *obj, 
			linestack_marker *lm_from, linestack_marker *lm_to, linestack_traverse_fn hl,
			void *userdata)
{
	struct traverse_object_data d;
	if (!current_backend) return FALSE;

	d.userdata = userdata;
	d.handler = hl;
	
	return linestack_traverse(n, lm_from, lm_to, traverse_object_handler, &d);
}

void linestack_free_marker(linestack_marker *lm)
{
	if (!lm) return;
	g_assert(current_backend);
	if (!current_backend->free_marker) return;
	current_backend->free_marker(lm);
}

linestack_marker *linestack_get_marker(struct network *n)
{
	if (!current_backend) return NULL;
	g_assert(current_backend->get_marker);
	return current_backend->get_marker(n);
}

static const char *linestack_messages[] = { 
	"NICK", "JOIN", "QUIT", "PART", "PRIVMSG", "NOTICE", "KICK", 
	"MODE", "TOPIC", NULL };

gboolean linestack_insert_line(const struct network *n, const struct line *l, enum data_direction dir)
{
	int i;
	gboolean needed = FALSE;

	if (l->argc == 0) return FALSE;

	if (!current_backend) return FALSE;
	g_assert(current_backend->insert_line);

	/* Only need PRIVMSG and NOTICE messages we send ourselves */
	if (dir == TO_SERVER && 
		g_strcasecmp(l->args[0], "PRIVMSG") && 
		g_strcasecmp(l->args[0], "NOTICE")) return FALSE;

	for (i = 0; linestack_messages[i]; i++) 
		if (!g_strcasecmp(linestack_messages[i], l->args[0]))
			needed = TRUE;

	if (!needed) return FALSE;

	return current_backend->insert_line(n, l);
}

static void send_line(struct line *l, time_t t, void *_client)
{
	struct client *c = _client;
	client_send_line(c, l);
}

gboolean linestack_send(struct network *n, linestack_marker *mf, linestack_marker *mt, const struct client *c)
{
	return linestack_traverse(n, mf, mt, send_line, c);
}

gboolean linestack_send_object(struct network *n, const char *obj, linestack_marker *mf, linestack_marker *mt, const struct client *c)
{
	return linestack_traverse_object(n, obj, mf, mt, send_line, c);
}

static void replay_line(struct line *l, time_t t, void *state)
{
	struct network_state *st = state;
	state_handle_data(st, l);
}

gboolean linestack_replay(struct network *n, linestack_marker *mf, linestack_marker *mt, struct network_state *st)
{
	return linestack_traverse(n, mf, mt, replay_line, st);
}
