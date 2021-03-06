/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2008 Jelmer Vernooĳ <jelmer@jelmer.uk>

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

#ifndef __LIBIRC_UTIL_H__
#define __LIBIRC_UTIL_H__

G_MODULE_EXPORT int base_strcmp(const char *a, const char *b);
G_MODULE_EXPORT int base_strncmp(const char *a, const char *b, size_t n);
G_MODULE_EXPORT int str_rfc1459cmp(const char *a, const char *b);
G_MODULE_EXPORT int str_strictrfc1459cmp(const char *a, const char *b);
G_MODULE_EXPORT int str_asciicmp(const char *a, const char *b);
G_MODULE_EXPORT int str_rfc1459ncmp(const char *a, const char *b, size_t n);
G_MODULE_EXPORT int str_strictrfc1459ncmp(const char *a, const char *b, size_t n);
G_MODULE_EXPORT int str_asciincmp(const char *a, const char *b, size_t n);
G_GNUC_WARN_UNUSED_RESULT G_MODULE_EXPORT char *g_io_channel_ip_get_description(GIOChannel *ch);
G_GNUC_WARN_UNUSED_RESULT G_MODULE_EXPORT char *list_make_string(GList *);
G_MODULE_EXPORT const char *g_io_channel_unix_get_sock_error(GIOChannel *ioc);
G_MODULE_EXPORT gsize i_convert(const char *str, gsize len, GIConv cd, GString *out);

#endif /* __LIBIRC_UTIL_H__ */
