/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2007 Jelmer Vernooij <jelmer@nl.linux.org>

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

#ifndef __CTRLPROXY_NICKSERV_H__
#define __CTRLPROXY_NICKSERV_H__

/**
 * @file
 * @brief NickServ information maintainance
 */

/**
 * Nickname/password combination for a particular network or globally.
 */
struct nickserv_entry {
	char *network;
	char *nick;
	char *pass;
};

#endif /* __CTRLPROXY_NICKSERV_H__ */
