#include "internals.h"

int slang_connect(char *host, int *port, char *nick, char *pass) 
{
	struct server *s;
	s = connect_to_server(host, port, nick, pass);
	if(!s)return 0;
	return s->id;
}

int slang_disconnect(int *connid)
{
	struct server *s = find_server(*connid);
	return close_server(s);
}

// load_module
int slang_load_module(int *connid, char *module, char *parameters)
{
	struct server *s = find_server(*connid);

	if(load_module(s, module, parameters))return 1;
	return 0;
}

// unload_module
int slang_unload_module(int *connid, char *module)
{
	struct server *s = find_server(*connid);
}

// join
int slang_join(int *connid, char *channels)
{

}

