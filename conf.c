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

char *first_section(void)
{
	if(global_section)return global_section->name;
	return NULL;
}

char *next_section(char *last_section)
{
	struct confsection *s = global_section;

	while(s) {
		if((!last_section && !s->name) || (last_section && s->name && !strcmp(last_section, s->name))){
			if(s->next)return s->next->name;
			return NULL;
		}
		s = s->next;
	}
	return NULL;
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
