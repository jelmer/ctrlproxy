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

struct module_context *load_module(struct server *s, char *name, char *args) 
{
	void *handle;
	struct module_functions *f;
	struct module_context *c;
	int *version;
	handle = dlopen(name, RTLD_LAZY);

	if(!handle) 
	{
		fprintf(stderr, "Error loading %s: %s\n", name, dlerror());
		return NULL;
	}

	version = dlsym(handle, "ctrlproxy_module_version");

	if(!version)
	{
		fprintf(stderr, "Error loading %s(while figuring out interface version): %s\n", name, dlerror());
		return NULL;
	}

	if(*version != CTRLPROXY_INTERFACE_VERSION) 
	{
		fprintf(stderr, "Module versioning mismatch: module has version %d, while ctrlproxy has %d\n", *version, CTRLPROXY_INTERFACE_VERSION);
		return NULL;
	}

	f = dlsym(handle, "ctrlproxy_functions");

	if(!f)
	{
		fprintf(stderr, "Error loading %s: %s\n", name, dlerror());
		return NULL;
	}

	c = malloc(sizeof(struct module_context));
	memset(c, 0, sizeof(struct module_context));

	DLIST_ADD(s->handlers, c);
	assert(s->handlers);
	c->handle = handle;
	c->functions = f;
	c->parent = s;
	active_context = c;
	if(c->functions->init)c->functions->init(c, args);
	active_context = NULL;
	return c;
}

int unload_module(struct module_context *c)
{
	if(c->functions->fini && !c->closed) {
		/* Don't call fini twice - if it crashes, 
		 * we might have a deadlock in the SIGSEGV handler... */
		c->closed = 1;
		active_context = c;
		c->functions->fini(c);
		active_context = NULL;
	}
	dlclose(c->handle);
	DLIST_REMOVE(c->parent->handlers, c);
	free(c);
	return 0;
}
