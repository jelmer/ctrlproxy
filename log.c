#include "ctrlproxy.h"

void minit(struct module_context *c)
{
	printf("Initialised!\n");
}

void mloop(struct module_context *c)
{
}

void mhandle_data(struct module_context *c, const struct line *l)
{
	printf("Raw data: %s", l->raw);
}

void mfinish(struct module_context *c)
{
	printf("Finished!\n");
}

struct module_functions functions = {
	minit, mloop, mhandle_data, mfinish
};
