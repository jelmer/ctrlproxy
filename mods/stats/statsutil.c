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
	g_hash_table_destroy(n->channels);
	g_free(n);
}

static void free_channel(void *_c) 
{
	struct channel *c = (struct channel *)_c;
	g_hash_table_destroy(c->properties);
	g_hash_tabel_destroy(c->nicks);
	g_free(c);
}

static void free_nick(void *_n) 
{
	struct nick *n = (struct nick *)_n;
	g_hash_table_destroy(n->properties);
	g_free(n);
}

static struct network *find_add_network(char *name)
{
	struct network *n = g_hash_table_lookup(networks, name);
	if(!n) { 
		n = calloc(1, sizeof(struct network *));
		n->channels = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, free_channel);
		g_hash_table_insert(networks, g_strdup(name), n);
	}
	return n;
}

static struct channel *find_add_channel(struct network *n, char *name)
{
	struct channel *c = g_hash_table_lookup(n->channels, name);
	if(!c) { 
		c = calloc(1, sizeof(struct channel *));
		c->network = n;
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
		n = calloc(1, sizeof(struct nick *));
		n->channel = c;
		n->properties = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
		g_hash_table_insert(c->nicks, g_strdup(name), n);
	}
	return n;
}

static void nick_set_property(struct nick *n, char *property, void *value)
{
	g_hash_table_insert(n->properties, property, value);
}

static void channel_set_property(struct channel *n, char *property, void *value)
{
	g_hash_table_insert(n->properties, property, value);
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

void stats_parse_file(char *f)
{
	TDB_CONTEXT *tdb_context;

	tdb_context = tdb_open(f, 0, 0, O_RDONLY, 00700);

	tdb_traverse(tdb_context, traverse_keys, NULL);
	return;
}
