/*
	ctrlproxy: A modular IRC proxy
	help: module for access the help system

	(c) 2006-2007 Jelmer Vernooij <jelmer@nl.linux.org>
	(c) 2004 Wilmer van der Gaast <wilmer@gaast.net>

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
#include "help.h"
#include "log.h"

#define BUFSIZE 1100

void help_free(help_t *h)
{
	if (h->file != NULL)
#if GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION >= 8
		g_mapped_file_free(h->file);
#else
		g_free(h->file);
#endif
	if (h->entries != NULL)
		g_hash_table_destroy(h->entries);
	g_free(h);
}

GHashTable *help_build_hash(char *data, gsize len)
{
	GHashTable *h = g_hash_table_new_full(g_str_hash, g_str_equal,
										  g_free, NULL);
	gsize i;
	char *p;

	i = 0;
	while (i < len) {
		if (data[i] != '?') {
			log_global(LOG_WARNING, "Unknown character '0x%02x' in help file", 
					   data[i]);
			g_hash_table_destroy(h);
			return NULL;
		}
		/* Key starts here */
		p = g_strstr_len(data+i, len-i, "\n");
		if (p == NULL) {
			log_global(LOG_WARNING, "Error parsing help file");
			g_hash_table_destroy(h);
			return NULL;
		}
		g_hash_table_insert(h, g_strndup(data+i+1, p-(data+i)-1), p+1);
		p = g_strstr_len(data+i, len-i, "\n%\n");
		if (p == NULL) {
			log_global(LOG_WARNING, "Error parsing help file");
			g_hash_table_destroy(h);
			return NULL;
		}
		i = p-data+3;
	}
	
	return h;
}

help_t *help_load_file( const char *helpfile )
{
	help_t *h;
	char *data;
	GError *error = NULL;
	gsize len;
	
	h = g_new0 (help_t, 1);
	
#if GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION >= 8
	h->file = g_mapped_file_new(helpfile, FALSE, &error);
	if (h->file != NULL) {
		len = g_mapped_file_get_length(h->file);
		data = g_mapped_file_get_contents(h->file);
	}
	
	if( h->file == NULL ) {
		log_global(LOG_WARNING, "Unable to open help file `%s': %s", helpfile, 
				  error->message);
		help_free( h );
		return NULL;
	}
#else
	if (!g_file_get_contents(helpfile, &h->file, &len, &error)) {
		log_global(LOG_WARNING, "Unable to open help file `%s': %s", helpfile, 
				  error->message);
		help_free( h );
		return NULL;
	} 
	data = h->file;
#endif

	h->entries = help_build_hash(data, len);

	return h;
}

const char *help_get(help_t *h, const char *string)
{
	if (h == NULL)
		return NULL;
	return g_hash_table_lookup(h->entries, string);
}
