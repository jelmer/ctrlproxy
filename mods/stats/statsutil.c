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

#define _GNU_SOURCE
#include "statsutil.h"
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <tdb.h>

GHashTable *networks;

static void free_network(void *_n) 
{
	struct network *n = (struct network *)_n;
	g_free(n->name);
	g_hash_table_destroy(n->channels);
	g_free(n);
}

static void free_channel(void *_c) 
{
	struct channel *c = (struct channel *)_c;
	g_free(c->name);
	g_hash_table_destroy(c->properties);
	g_hash_table_destroy(c->nicks);
	g_free(c);
}

static void free_nick(void *_n) 
{
	struct nick *n = (struct nick *)_n;
	g_free(n->name);
	g_hash_table_destroy(n->properties);
	g_free(n);
}

static struct network *find_add_network(char *name)
{
	struct network *n = g_hash_table_lookup(networks, name);
	if(!n) { 
		n = calloc(1, sizeof(struct network));
		n->name = g_strdup(name);
		n->channels = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, free_channel);
		g_hash_table_insert(networks, g_strdup(name), n);
	}
	return n;
}

static struct channel *find_add_channel(struct network *n, char *name)
{
	struct channel *c = g_hash_table_lookup(n->channels, name);
	if(!c) { 
		c = calloc(1, sizeof(struct channel));
		c->network = n;
		c->name = g_strdup(name);
		c->nicks = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, free_nick);
		c->properties = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
		g_hash_table_insert(n->channels, g_strdup(name), c);
	}
	return c;
}

static struct nick *find_add_nick(struct channel *c, char *name)
{
	struct nick *n = g_hash_table_lookup(c->nicks, name);
	if(!n) { 
		n = calloc(1, sizeof(struct nick));
		n->channel = c;
		n->name = g_strdup(name);
		n->properties = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
		g_hash_table_insert(c->nicks, g_strdup(name), n);
	}
	return n;
}

static void nick_set_property(struct nick *n, char *property, void *value)
{
	long *l = calloc(1, sizeof(STAT_VALUE));
	*l = *(long *)value;
	g_hash_table_insert(n->properties, g_strdup(property), l);
}

static void channel_set_property(struct channel *n, char *property, void *value)
{	
	long *l = calloc(1, sizeof(STAT_VALUE));
	*l = *(long *)value;
	g_hash_table_insert(n->properties, g_strdup(property), l);
}

STAT_VALUE nick_get_property(struct nick *n, char *p)
{
	STAT_VALUE *r = (STAT_VALUE *)g_hash_table_lookup(n->properties, p);
	if(!r) return 0;
	return *r;
}

STAT_VALUE channel_get_property(struct channel *n, char *p)
{
	STAT_VALUE *r = (STAT_VALUE *)g_hash_table_lookup(n->properties, p);
	if(!r) return 0;
	return *r;
}

static int traverse_keys(TDB_CONTEXT *tdb_context, TDB_DATA key, TDB_DATA value, void *pattern)
{
	struct network *network;
	struct channel *channel;
	struct nick *nick;
	char **keys;

	/* Split up key */
	keys = g_strsplit(key.dptr, "/", 4);
	if(!keys[0]) return 0;
	network = find_add_network(keys[0]);
	if(!keys[1]) return 0;
	channel = find_add_channel(network, keys[1]);
	if(!keys[2]) return 0;
	if(keys[3]) { 
		nick = find_add_nick(channel, keys[2]);
		nick_set_property(nick, keys[3], value.dptr);
	} else {
		channel_set_property(channel, keys[2], value.dptr);
	}
	
	g_strfreev(keys);
	return 0;
}

void stats_fini()
{
	g_hash_table_destroy(networks);
}

void stats_init() 
{
	networks = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, free_network);
}

void stats_parse_file(const char *f)
{
	TDB_CONTEXT *tdb_context;

	tdb_context = tdb_open(f, 0, 0, O_RDONLY, 00700);

	tdb_traverse(tdb_context, traverse_keys, NULL);
	return;
}

static char *sortpropertyname = NULL;

static gint sort_property(gconstpointer a, gconstpointer b)
{
	struct nick *n1 = (struct nick *)a;
	struct nick *n2 = (struct nick *)b;
	
	STAT_VALUE v1 = nick_get_property(n1, sortpropertyname);
	STAT_VALUE v2 = nick_get_property(n2, sortpropertyname);

	if(v1 > v2) return 1;
	if(v1 < v2) return -1;
	return 0;
}

static void hash2list_sorted(gpointer key, gpointer value, gpointer user_data)
{
	GList **ret = (GList **)user_data;

	*ret = g_list_insert_sorted(*ret, value, sort_property);
}

GList *get_nicks_sorted_by_property(struct channel *c, char *property)
{
	GList *ret = NULL;

	sortpropertyname = property;
	
	g_hash_table_foreach(c->nicks, hash2list_sorted, &ret);
	return ret;
}

static void hash_highest(gpointer key, gpointer value, gpointer user_data)
{
	static STAT_VALUE max;
	STAT_VALUE this;
	struct nick *n = (struct nick *)value;
	struct nick **ret = (struct nick **)user_data;

	if(*ret == NULL) max = 0;

	this = nick_get_property(n, sortpropertyname);
	if(this > max) { *ret = n; max = this; }
}

struct nick *get_highest_nick_for_property(struct channel *c, char *p)
{
	struct nick *ret = NULL;
	sortpropertyname = p;
	g_hash_table_foreach(c->nicks, hash_highest, &ret);
	return ret;
}

struct network *get_network(char *name)
{
	return (struct network *)g_hash_table_lookup(networks, name);
}

struct channel *get_channel(struct network *n, char *name)
{
	return (struct channel *)g_hash_table_lookup(n->channels, name);
}
