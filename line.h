/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2005 Jelmer Vernooij <jelmer@nl.linux.org>

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

#ifndef __CTRLPROXY_LINE_H__
#define __CTRLPROXY_LINE_H__

#include <gmodule.h>

struct line {
	enum {
		LINE_IS_PRIVATE = 1,
		LINE_DONT_SEND = 2,
		LINE_STOP_PROCESSING = 4,
		LINE_NO_LOGGING = 8
	} options;
	char *origin;
	char **args; /* NULL terminated */
	size_t argc;
	enum { COLON_UNKNOWN = 0, WITH_COLON = 1, WITHOUT_COLON = 2 } has_endcolon;
};

G_MODULE_EXPORT struct line *linedup(struct line *l);
G_MODULE_EXPORT struct line * irc_parse_line(const char *data);
G_MODULE_EXPORT struct line * virc_parse_line(const char *origin, va_list ap);
G_MODULE_EXPORT char *irc_line_string(const struct line *l);
G_MODULE_EXPORT char *irc_line_string_nl(const struct line *l);
G_MODULE_EXPORT char *line_get_nick(struct line *l);
G_MODULE_EXPORT void free_line(struct line *l);
G_MODULE_EXPORT gboolean irc_send_args(GIOChannel *, ...);
G_MODULE_EXPORT gboolean irc_sendf(GIOChannel *, char *fmt, ...);
G_MODULE_EXPORT int irc_send_line(GIOChannel *, const struct line *l);
G_MODULE_EXPORT struct line *irc_parse_linef( char *origin, ... );
G_MODULE_EXPORT struct line *irc_parse_line_args( char *origin, ... );
G_MODULE_EXPORT GIOStatus irc_recv_line(GIOChannel *c, GError **err, struct line **);

#endif /* __CTRLPROXY_LINE_H__ */
