/* 
	ctrlproxy: A modular IRC proxy
	(c) 2002 Jelmer Vernooij <jelmer@nl.linux.org>

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

struct confsection {
	struct confsection *prev, *next;
	char *name;
	char **values;
	int no_values;
};

struct confsection *global_section;

int load_conf_file(char *file) 
{
	FILE *in = fopen(file, "r");
	size_t len;
	char *line = NULL;
	char *p, *t;
	struct confsection *c;
	int lineno = 0;

	global_section = malloc(sizeof(struct confsection));

	memset(global_section, 0, sizeof(struct confsection));

	c = global_section;

	if(!in){
		fprintf(stderr, "Unable to open %s!\n", file);
		return -1;
	}

	while(getline(&line, &len, in) != -1) 
	{
		if(!line)break;
		lineno++;
		p = line;
		/* Skip all initial whites... */
		while(*p == ' ' || *p == '\t')p++;

		if((t = strchr(p, '\n')))*t = '\0';
		
		if((t = strchr(p, '='))) {
			c->values = realloc(c->values, (c->no_values+2) * sizeof(char *));
			c->values[c->no_values] = strdup(p);
			c->no_values++;
		} else if(*p == '[') {
			p++;
			c = malloc(sizeof(struct confsection));
			memset(c, 0, sizeof(struct confsection));
			DLIST_ADD(global_section, c);
			t = strchr(p, ']');
			if(!t) {
				fprintf(stderr, "Error parsing line %d in %s\n", lineno, file);
				return -1;
			}
			*t = '\0';
			c->name = strdup(p);
		} else {
			while(*p == ' ' || *p == '\t'){
				if(*p == '#')p+=strlen(p);
				p++;
			}
			if(*p != '\0') {
				fprintf(stderr, "Malformed line %d in %s\n", lineno, file);
				return -1;
			}
		}
		free(line); line = NULL;
	}
	return 0;
}

char **enum_sections(void)
{
	static char **enums = NULL;
	int nr_entries = 0;
	struct confsection *s = global_section;

	while(s) {
		if(s->name) {
			enums = realloc(enums, (nr_entries+2) * sizeof(char *));
			enums[nr_entries] = s->name;
			nr_entries++;
		}
		s = s->next;
	}
	enums = realloc(enums, (nr_entries+2) * sizeof(char *));
	enums[nr_entries] = NULL;
	return enums;
}

char *get_conf(char *section, char *name)
{
	struct confsection *s = global_section;
	int i;
	char *t;

	while(s) {
		if(s->name == section || (s->name && section && !strcmp(section, s->name))) {
			for(i = 0; i < s->no_values; i++) {
				if(!strncmp(s->values[i], name, strlen(name))){
					t = strchr(s->values[i], '=')+1;
					while(*t == ' ' || *t == '\t')t++;
					return t;
				}
			}
		}
		s = s->next;
	}

	if(section != NULL) return get_conf(NULL, name);
	return NULL;
}
