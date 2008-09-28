/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2006 Jelmer Vernooij <jelmer@nl.linux.org>

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

#ifndef HAVE_DAEMON_DAEMON_H
#define HAVE_DAEMON_DAEMON_H

struct ctrlproxyd_config {
	char *ctrlproxy_path;
	char *port;
	char *address;
	char *configdir;
};

struct login_details {
	char *password;
	char *servername;
	char *servicename;
	char *nick;
	char *username;
	char *mode;
	char *unused;
	char *realname;
};

#endif
