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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "internals.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <netdb.h>

/* globals */
char my_hostname[MAXHOSTNAMELEN+2];

const char *get_modules_path() { return MODULESDIR; }
const char *get_shared_path() { return SHAREDIR; }
const char *get_my_hostname() { return my_hostname; }
const char *ctrlproxy_version() { return PACKAGE_VERSION; }