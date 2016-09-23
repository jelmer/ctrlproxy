/*
	ctrlproxy: A modular IRC proxy
	(c) 2007 Jelmer Vernooĳ <jelmer@jelmer.uk>

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

#ifndef __LIBIRC_URL_H__
#define __LIBIRC_URL_H__

G_GNUC_WARN_UNUSED_RESULT char *irc_create_url(const char *server, const char *port, gboolean ssl);
G_GNUC_WARN_UNUSED_RESULT gboolean irc_parse_url(const char *url, char **server, char **port, gboolean *ssl);

#endif /* __LIBIRC_URL_H__ */
