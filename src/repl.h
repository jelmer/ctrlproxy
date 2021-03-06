/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2005 Jelmer Vernooĳ <jelmer@jelmer.uk>

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

#ifndef __CTRLPROXY_REPL_H__
#define __CTRLPROXY_REPL_H__

/**
 * @file
 * @brief Replication
 */

/**
 * A replication backend
 */
struct replication_backend {
	/** Name of the backend, as can be specified in replication=. */
	const char *name;
	/** Replication function. */
	void (*replication_fn) (struct irc_client *);
};

void register_replication_backend(const struct replication_backend *);
struct replication_backend *repl_find_backend(const char *name);

#endif /* __CTRLPROXY_REPL_H__ */
