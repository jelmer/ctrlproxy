#include "internals.h"

int load_conf_file(char *file) 
{
	FILE *in = fopen(file, "r");
	size_t len;
	char *line = NULL;
	int level = 0;

	if(!in)return -1;

	while(getline(&line, &len, in) != -1) 
	{


		

	}
	


}

char *get_conf_context(struct module_context *c, char *name)
{

}

char *get_conf_server(struct server *s, char *name)
{

}

char *get_conf_global(char *name)
{

}
