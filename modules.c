#include "internals.h"

struct module_context *load_module(struct server *s, char *name) 
{
	void *handle;
	struct module_functions *f;
	struct module_context *c;
	handle = dlopen(name, RTLD_LAZY);

	if(!handle) 
	{
		fprintf(stderr, "Error loading %s: %s\n", name, dlerror());
		return NULL;
	}

	f = dlsym(handle, "functions");

	if(!f)
	{
		fprintf(stderr, "Error loading %s: %s\n", name, dlerror());
		return NULL;
	}

	c = malloc(sizeof(struct module_context));
	memset(c, 0, sizeof(struct module_context));

	DLIST_ADD(s->handlers, c);
	c->handle = handle;
	c->functions = f;
	c->parent = s;
	if(c->functions->init)c->functions->init(c);
	return c;
}

int unload_module(struct module_context *c)
{
	if(c->functions->fini)c->functions->fini(c);
	dlclose(c->handle);
	DLIST_REMOVE(c->parent->handlers, c);
	free(c);
	return 0;
}
