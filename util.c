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

xmlNodePtr xmlFindChildByElementName(xmlNodePtr parent, const xmlChar *name)
{
	xmlNodePtr cur = parent->xmlChildrenNode;
	while(cur) {
		if(!xmlIsBlankNode(cur) && !strcmp(cur->name, name))return cur;
		cur = cur->next;
	}
	return NULL;
}

xmlNodePtr xmlFindChildByName(xmlNodePtr parent, const xmlChar *name) 
{
	xmlNodePtr cur = parent->xmlChildrenNode;
	while(cur) {
		char *nname = xmlGetProp(cur, "name");
		
		if(xmlIsBlankNode(cur) || !strcmp(cur->name, "comment")) { cur = cur->next; continue; }

		if(nname && !strcmp(nname, name)) { xmlFree(nname); return cur; }

		xmlFree(nname);
		cur = cur->next;
	}
	return NULL;
}

char *list_make_string(char **list)
{
	int i;
	size_t len = 20;
	char *ret;
	/* First, calculate the length */
	for(i = 0; list[i]; i++) len+=strlen(list[i])+1;

	ret = malloc(sizeof(char *) * len);
	ret[0] = '\0';

	for(i = 0; list[i]; i++) 
	{ 
		strncat(ret, list[i], len);
		strncat(ret, " ", len); 
	}

	return ret;
}

char *ctrlproxy_path(char *part)
{
	char *p, *p1;
	asprintf(&p, "%s/.ctrlproxy", g_get_home_dir());
	if(mkdir(p, 0700) != 0 && errno != EEXIST) {
		g_warning(_("Couldn't create '%s'!\n"), p);
		free(p);
		exit(1);
	}

	asprintf(&p1, "%s/%s", p, part);
	free(p);
	return p1;
}

int strrfc1459cmp(const char *a, const char *b)
{
	int i;
	for(i = 0; a[i] && b[i]; i++) {
		if(a[i] - b[i] == 0) continue; 
		switch(a[i]) {
				case 'a'...'b':
					if(tolower(a[i]) - tolower(b[i]))	
						return tolower(a[i]) - tolower(b[i]);
					break;
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
					return a[i] - b[i];
		}
	}
	return 0;
}
