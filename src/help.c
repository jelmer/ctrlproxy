/*
	ctrlproxy: A modular IRC proxy
	help: module for access the help system

	(c) 2006 Jelmer Vernooij <jelmer@nl.linux.org>
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
	g_mapped_file_free(h->file);
	g_hash_table_destroy(h->entries);
	g_free(h);
}

help_t *help_init( const char *helpfile )
{
	help_t *h;
	char *data;
	GError *error = NULL;
	gsize len, i, k;
	char *p;
	
	h = g_new0 (help_t, 1);
	
	h->file = g_mapped_file_new(helpfile, TRUE, &error);
	if (h->file != NULL) {
		len = g_mapped_file_get_length(h->file);
		data = g_mapped_file_get_contents(h->file);
	}
	
	if( h->file == NULL ) {
		log_global(LOG_WARNING, "Unable to open help file `%s': %s", helpfile, 
				  error->message);
		g_free( h );
		return NULL;
	}

	h->entries = g_hash_table_new(g_str_hash, g_str_equal);
	i = 0;
	while (i < len) {
		if (data[i] == '?') {
			/* Key starts here */
			k = i+1;
			p = g_strstr_len(data+k, len-k, "\n%\n");
			if (p == NULL) {
				log_global(LOG_WARNING, "Error parsing help file");
				goto error;
			}
			p[1] = 0;
			i+=strlen(data+i+3);
			p = g_strstr_len(data+k, len-k, "\n");
			p[0] = 0;
			g_hash_table_insert(h->entries, data+k, p+1);
		} else {
			log_global(LOG_WARNING, "Unknown character '%c' in help file", 
					   data[i]);
			i++;
		}
	}

	return h;

error:
	g_mapped_file_free(h->file);
	g_hash_table_destroy(h->entries);
	g_free(h);
	return NULL;
}

const char *help_get(help_t *h, const char *string)
{
	return g_hash_table_lookup(h->entries, string);
}
