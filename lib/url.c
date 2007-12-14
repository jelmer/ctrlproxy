/*
	ctrlproxy: A modular IRC proxy
	(c) 2007 Jelmer Vernooij <jelmer@nl.linux.org>

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


#include "internals.h"

char *irc_create_url(const char *server, const char *port, gboolean ssl)
{
	if (ssl && (!strcmp("ircs", port) || !strcmp("994", port)))
		return g_strdup_printf("ircs://%s", server);
	if (!ssl && (!strcmp("ircd", port) || !strcmp("6667", port)))
		return g_strdup_printf("irc://%s", server);
	return g_strdup_printf("%s://%s:%s", (ssl?"ircs":"irc"), server, port);
}

gboolean irc_parse_url(const char *url, char **server, char **port, gboolean *ssl)
{
	char *p;

	if (!strncmp(url, "irc://", strlen("irc://"))) {
		*ssl = FALSE;
		url += strlen("irc://");
	} else if (!strncmp(url, "ircs://", strlen("ircs://"))) {
		*ssl = TRUE;
		url += strlen("ircs://");
	} else if (strstr(url, "://")) {
		*server = NULL;
		*port = NULL;
		*ssl = FALSE;
		return FALSE;
	} else {
		*ssl = FALSE;
	}

	p = strchr(url, ':');
	if (p != NULL) {
		*port = g_strdup(p+1);
		*server = g_strndup(url, p-url);
		return TRUE;
	} 
	
	if (*ssl) {
		*port = g_strdup("ircs");
	} else {
		*port = g_strdup("ircd");
	}

	*server = g_strdup(url);

	return TRUE;
}
