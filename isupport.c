/*
	ctrlproxy: A modular IRC proxy
	(c) 2005 Jelmer Vernooij <jelmer@nl.linux.org>

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

#define DEFAULT_PREFIX		"(ov)@+"
#define DEFAULT_CHANTYPES 	"#&"

void handle_005(struct network_state *s, struct line *l)
{
	unsigned int i;
	
	g_assert(s);
	g_assert(s->info);
	g_assert(l);

	g_assert(l->argc >= 1);

	for(i = 3; i < l->argc-1; i++) {
		char *sep;
		char *key, *val;

		g_assert(l->args[i]);

		sep = strchr(l->args[i], '=');

		if (!sep) { 
			key = g_strdup(l->args[i]); 
			val = NULL; 
		} else {
			key = g_strndup(l->args[i], sep - l->args[i]);
			val = g_strdup(sep+1);
		}
		
		g_hash_table_replace(s->info->features, key, val);

		if(!g_strcasecmp(key, "CASEMAPPING")) {
			if(!g_strcasecmp(val, "rfc1459")) {
				s->info->casemapping = CASEMAP_RFC1459;
			} else if(!g_strcasecmp(val, "strict-rfc1459")) {
				s->info->casemapping = CASEMAP_STRICT_RFC1459;
			} else if(!g_strcasecmp(val, "ascii")) {
				s->info->casemapping = CASEMAP_ASCII;
			} else {
				s->info->casemapping = CASEMAP_UNKNOWN;
				log_network_state(s, LOG_WARNING, "Unknown CASEMAPPING value '%s'", val);
			}
		} else if(!g_strcasecmp(key, "NETWORK")) {
			g_free(s->info->name);
			s->info->name = g_strdup(val);
		} 
	}
}

gboolean network_supports(const struct network_info *n, const char *fe)
{
	gpointer k, v;
	g_assert(n);
	g_assert(n->features);
	return g_hash_table_lookup_extended (n->features, fe, &k, &v);
}

int irccmp(const struct network_info *n, const char *a, const char *b)
{
	switch(n?n->casemapping:CASEMAP_UNKNOWN) {
	default:
	case CASEMAP_UNKNOWN:
	case CASEMAP_RFC1459:
		return str_rfc1459cmp(a,b);
	case CASEMAP_ASCII:
		return str_asciicmp(a,b);
	case CASEMAP_STRICT_RFC1459:
		return str_strictrfc1459cmp(a,b);
	}

	return 0;
}

gboolean is_channelname(const char *name, const struct network_info *n)
{
	const char *chantypes = NULL;

	g_assert(name);

	if (n != NULL) {
		g_assert(n->features);
	
		chantypes = g_hash_table_lookup(n->features, "CHANTYPES");
	}

	if(chantypes == NULL) 
		chantypes = DEFAULT_CHANTYPES;
	
	if(strchr(chantypes, name[0])) 
		return TRUE;

	return FALSE;
}

gboolean is_prefix(char p, const struct network_info *n)
{
	const char *prefix = NULL;
	const char *pref_end;
	
	if (n != NULL) {
		g_assert(n->features);
		prefix = g_hash_table_lookup(n->features, "PREFIX");
	}
	
	if (prefix == NULL) 
		prefix = DEFAULT_PREFIX;

	pref_end = strchr(prefix, ')');
	if (!pref_end)pref_end = prefix; else pref_end++;

	if(strchr(pref_end, p)) return TRUE;
	return FALSE;
}

char get_prefix_by_mode(char mode, const struct network_info *n)
{
	const char *prefix = NULL;
	int i;
	char *pref_end;

	if (n == NULL) {
		g_assert(n->features);

		prefix = g_hash_table_lookup(n->features, "PREFIX");
	}

	if (prefix == NULL) prefix = DEFAULT_PREFIX;
	
	pref_end = strchr(prefix, ')');
	if(prefix[0] != '(' || !pref_end) {
		log_global(NULL, LOG_WARNING, "Malformed PREFIX data `%s'", prefix);
		return ' ';
	}
	pref_end++;
	prefix++;

	for(i = 0; pref_end[i]; i++) {
		if(prefix[i] == mode) return pref_end[i];
	}
	return ' ';
}
