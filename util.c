/* 
	ctrlproxy: A modular IRC proxy
	(c) 2002-2003 Jelmer Vernooij <jelmer@nl.linux.org>

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

#include "internals.h"
#include <errno.h>
#include <ctype.h>

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
