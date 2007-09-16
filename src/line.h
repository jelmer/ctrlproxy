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

#ifndef G_MODULE_EXPORT
#define G_MODULE_EXPORT
#endif

/**
 * @file
 * @brief Line manipulation
 */

enum has_endcolon { COLON_UNKNOWN = 0, WITH_COLON = 1, WITHOUT_COLON = 2 } ;

/**
 * Line information.
 */
struct line {
	char *origin;
	char **args; /* NULL terminated */
	size_t argc;
	enum has_endcolon has_endcolon;
};

/** 
 * Copy a line
 */
G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT G_MODULE_EXPORT struct line *linedup(const struct line *l);
G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT G_MODULE_EXPORT struct line *irc_parse_line(const char *data);
G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT G_MODULE_EXPORT struct line *virc_parse_line(const char *origin, va_list ap);
G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT G_MODULE_EXPORT char *irc_line_string(const struct line *l);
G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT G_MODULE_EXPORT char *irc_line_string_nl(const struct line *l);
G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT G_MODULE_EXPORT char *line_get_nick(const struct line *l);
G_MODULE_EXPORT void free_line(struct line *l);
G_GNUC_NULL_TERMINATED G_GNUC_WARN_UNUSED_RESULT G_MODULE_EXPORT GIOStatus irc_send_args(GIOChannel *, GIConv, GError **, ...);
G_GNUC_PRINTF(4, 5) G_GNUC_WARN_UNUSED_RESULT G_MODULE_EXPORT GIOStatus irc_sendf(GIOChannel *, GIConv, GError **, char *fmt, ...);
G_GNUC_WARN_UNUSED_RESULT G_MODULE_EXPORT GIOStatus irc_send_line(
										GIOChannel *, GIConv,
										const struct line *l,
                                        GError **);
G_GNUC_NULL_TERMINATED G_GNUC_WARN_UNUSED_RESULT G_MODULE_EXPORT struct line *irc_parse_linef(
													const char *origin, ... );
G_GNUC_NULL_TERMINATED G_GNUC_WARN_UNUSED_RESULT G_MODULE_EXPORT struct line *irc_parse_line_args(
													const char *origin, ... );

/**
 * Read a line from an IO Channel. This will return a line _with_ UTF-8 
 * characters only!
 */
G_GNUC_WARN_UNUSED_RESULT G_MODULE_EXPORT GIOStatus irc_recv_line(GIOChannel *c, GIConv iconv,
										GError **err, 
										struct line **);

G_MODULE_EXPORT gboolean line_add_arg(struct line *l, const char *arg);

#endif /* __CTRLPROXY_LINE_H__ */
