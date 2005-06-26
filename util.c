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
#ifdef _WIN32
#include <direct.h>
#define mkdir(s,t) _mkdir(s)
#endif

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
		strncat(ret, " ", len); 
	}

	return ret;
}

char *ctrlproxy_path(char *part)
{
	char *p, *p1;
	p = g_strdup_printf("%s/.ctrlproxy", g_get_home_dir());
	if(mkdir(p, 0700) != 0 && errno != EEXIST) {
		g_free(p);
		exit(1);
	}

	p1 = g_strdup_printf("%s/%s", p, part);
	g_free(p);
	return p1;
}

int strrfc1459cmp(const char *a, const char *b)
{
	int i;

	for(i = 0; ; i++) {
		if(a[i] == 0 && b[i] == 0) break;
		if(a[i] - b[i] == 0) continue; 
		switch(a[i]) {
				case '{':
					if(b[i] != '[') return a[i] - b[i];
					break;
				case '}':
					if(b[i] != ']') return a[i] - b[i];
					break;
				case '^':
					if(b[i] != '~') return a[i] - b[i];
					break;
				case '|':
					if(b[i] != '\\') return a[i] - b[i];
				default:
					if(a[i] >= 'a' && a[i] <= 'z' && tolower(a[i]) - tolower(b[i]))	
						return tolower(a[i]) - tolower(b[i]);
					return a[i] - b[i];
		}
	}
	return 0;
}
