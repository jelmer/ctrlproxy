#include <ctrlproxy.h>

gboolean init_plugin(struct plugin *p)
{   
	/* Do something */
	return TRUE;
}

gboolean fini_plugin(struct plugin *p)
{
	/* Free your data structures and unregister */
	return TRUE;
}
