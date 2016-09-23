/*
	ctrlproxy: A modular IRC proxy
	(c) 2006-2007 Jelmer Vernooĳ <jelmer@jelmer.uk>

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

#ifndef __CTRLPROXY_LOG_H__
#define __CTRLPROXY_LOG_H__

struct irc_network;
struct irc_client;

enum log_level { LOG_DATA=5, LOG_TRACE=4, LOG_INFO=3, LOG_WARNING=2, LOG_ERROR=1 };
G_GNUC_PRINTF(3, 4) G_MODULE_EXPORT void network_log(enum log_level, const struct irc_network *, const char *fmt, ...);
G_GNUC_PRINTF(2, 3) G_MODULE_EXPORT void log_global(enum log_level, const char *fmt, ...);

#endif /* __CTRLPROXY_LOG_H__ */
