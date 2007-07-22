/*
	ctrlproxy: A modular IRC proxy
	(c) 2006-2007 Jelmer Vernooij <jelmer@nl.linux.org>

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

#ifndef __CTRLPROXY_LOG_H__
#define __CTRLPROXY_LOG_H__

struct network;
struct client;

enum log_level { LOG_DATA=5, LOG_TRACE=4, LOG_INFO=3, LOG_WARNING=2, LOG_ERROR=1 };
G_MODULE_EXPORT void log_network(enum log_level, const struct network *, const char *fmt, ...);
G_MODULE_EXPORT void log_client(enum log_level, const struct client *, const char *fmt, ...);
G_MODULE_EXPORT void log_global(enum log_level, const char *fmt, ...);

#endif /* __CTRLPROXY_LOG_H__ */
