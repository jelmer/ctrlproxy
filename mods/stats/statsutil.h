/* 
	ctrlproxy: A modular IRC proxy
	(c) 2002 Jelmer Vernooij <jelmer@nl.linux.org>

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

#ifndef __CTRLPROXY_STATSUTIL_H__
#define __CTRLPROXY_STATSUTIL_H__

#include <glib.h>

struct network;
struct channel;

struct nick {
	GHashTable *properties;
	struct channel *channel;
};

struct channel {
	GHashTable *properties;
	struct network *network;
	GHashTable *nicks;
};

struct network {
	GHashTable *channels;
};

extern GHashTable *networks;

void stats_parse_file(char *f);
void stats_init();
void stats_fini();

#endif /* __CTRLPROXY_STATSUTIL_H__ */
