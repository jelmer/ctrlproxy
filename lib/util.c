/* 
	ctrlproxy: A modular IRC proxy
	(c) 2002-2008 Jelmer Vernooij <jelmer@nl.linux.org>

	g_mkdir_with_parents() imported from libglib and 
	(C) Red Hat.

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
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <glib/gstdio.h>

static inline int str_cmphelper(const char *a, const char *b, char sh, char sl, char eh, char el)
{
	int i;
	char h,l;
	for (i = 0; a[i] && b[i]; i++) {
		if (a[i] == b[i]) continue;
		l = (a[i]>b[i]?b[i]:a[i]);
		h = (a[i]>b[i]?a[i]:b[i]);

		if (h < sh || h > eh || l < sl || l > el) 
			break;

		if (h-sh != l-sl)
			break;
	}

	return a[i]-b[i];
}

int str_asciicmp(const char *a, const char *b)
{
	g_assert(a != NULL);
	g_assert(b != NULL);
	return str_cmphelper(a, b, 97, 65, 122, 90);
}

int str_strictrfc1459cmp(const char *a, const char *b)
{
	g_assert(a != NULL);
	g_assert(b != NULL);
	return str_cmphelper(a, b, 97, 65, 125, 93);
}


int str_rfc1459cmp(const char *a, const char *b)
{
	g_assert(a != NULL);
	g_assert(b != NULL);
	return str_cmphelper(a, b, 97, 65, 126, 94);
}

char *g_io_channel_ip_get_description(GIOChannel *ch)
{
	socklen_t len = sizeof(struct sockaddr_storage);
	struct sockaddr *sa = g_malloc(len);
	char hostname[NI_MAXHOST];
	char service[NI_MAXSERV];
	char *description = NULL;
	int fd = g_io_channel_unix_get_fd(ch);

	if (getpeername (fd, sa, &len) == 0 &&
		getnameinfo(sa, len, hostname, sizeof(hostname),
					service, sizeof(service), NI_NOFQDN | NI_NUMERICSERV) == 0) {

		description = g_strdup_printf("%s:%s", hostname, service);
	} else {
		log_global(LOG_WARNING, "Unable to obtain remote IP address: %s", 
				   strerror(errno));
	}

	g_free(sa);
	return description;
}

char *list_make_string(GList *list)
{
	size_t len = 20;
	char *ret;
	GList *gl;

	/* First, calculate the length */
	for(gl = list; gl; gl = gl->next) len+=strlen(gl->data)+1;

	ret = g_new(char,len);
	ret[0] = '\0';

	for(gl = list; gl; gl = gl->next) 
	{ 
		strncat(ret, gl->data, len);
		if (gl->next) strncat(ret, " ", len); 
	}

	return ret;
}


const char *g_io_channel_unix_get_sock_error(GIOChannel *ioc)
{
	int valopt;
	socklen_t valoptlen = sizeof(valopt);
	int fd = g_io_channel_unix_get_fd(ioc);

	getsockopt(fd, SOL_SOCKET, SO_ERROR, &valopt, &valoptlen);

	return strerror(valopt);
}

