/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2007 Jelmer Vernooij <jelmer@jelmer.uk>

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

#ifndef __CTRLPROXY_KEYFILE_H__
#define __CTRLPROXY_KEYFILE_H__

/**
 * @file
 * @brief Key file
 */

/**
 * Name/password combination for a particular network or globally.
 */
struct keyfile_entry {
	char *network;
	char *nick;
	char *pass;
};

gboolean keyfile_read_file(const char *filename, char commentchar, GList **nicks);
gboolean keyfile_write_file(GList *entries, const char *header, 
							const char *filename);

#endif /* __CTRLPROXY_KEYFILE_H__ */
