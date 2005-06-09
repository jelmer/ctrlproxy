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

#include "ctrlproxy.h"
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(s,t) _mkdir(s)
#endif

static char *data_path = NULL;
static GHashTable *networks = NULL;

static FILE *get_fd(const struct network *n)
{
	FILE *ch;

	ch = g_hash_table_lookup(networks, n);

	if (!ch) {
		char *path = g_strdup_printf("%s/%s", data_path, n->name);
		ch = fopen(path, "a+");
		if (!ch) {
			log_network("linestack_file", n, "Unable to open linestack file %s", path);
			g_free(path);
			return NULL;
		}
		g_free(path);
		g_hash_table_insert(networks, n, ch);
	}

	return ch;
}

static gboolean file_init(void)
{
	data_path = ctrlproxy_path("linestack_file");
	mkdir(data_path, 0700);
	networks = g_hash_table_new_full(NULL, NULL, NULL, (GDestroyNotify)fclose);
	return TRUE;
}

static gboolean file_fini(void)
{
	g_free(data_path); data_path = NULL;
	g_hash_table_destroy(networks);
	return TRUE;
}

static gboolean file_insert_line(const struct network *n, const struct line *l)
{
	FILE *ch = get_fd(n);
	char *raw;
	int ret;
	time_t t = time(NULL);
	if (!ch) return FALSE;

	if (fwrite(&t, sizeof(t), 1, ch) != 1)
		return FALSE;

	raw = irc_line_string_nl(l);
	ret = fputs(raw, ch);
	g_free(raw);

	return (ret != EOF);
}

static linestack_marker *file_get_marker(struct network *n)
{
	long *pos;
	FILE *ch = get_fd(n);
	if (!ch) return NULL;

	pos = g_new0(long, 1);
	*pos = ftell(ch);

	return pos;
}

static gboolean file_traverse(struct network *n, 
		linestack_marker *m,
		linestack_traverse_fn tf, 
		void *userdata)
{
	char raw[512];
	long *offset;
	FILE *ch = get_fd(n);
	if (!ch) return FALSE;

	/* Flush channel before reading otherwise data corruption may occur */
	fflush(ch);
	
	offset = m;

	/* Go back to begin of file */
	fseek(ch, *offset, SEEK_SET);
	
	while(!feof(ch)) 
	{
		time_t t;
		struct line *l;
		fread(&t, sizeof(t), 1, ch);
		fgets(raw, sizeof(raw), ch);
		l = irc_parse_line(raw);
		tf(l, t, userdata);
		free_line(l);
	}

	return TRUE;
}

static struct linestack_ops file = {
	.name = "file",
	.init = file_init,
	.fini = file_fini,
	.insert_line = file_insert_line,
	.get_marker = file_get_marker,
	.free_marker = g_free,
	.traverse = file_traverse
};

static gboolean fini_plugin(struct plugin *p) {
	unregister_linestack(&file);
	return TRUE;
}

static gboolean init_plugin(struct plugin *p) {
	register_linestack(&file);
	return TRUE;
}

struct plugin_ops plugin = {
	.name = "linestack_file",
	.version = 0,
	.init = init_plugin,
	.fini = fini_plugin
};
