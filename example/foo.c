#include <ctrlproxy.h>

gboolean init_plugin(void)
{
	/* Do something */
	return TRUE;
}

struct plugin_ops plugin = {
	.name = "example",
	.version = CTRLPROXY_PLUGIN_VERSION,
	.init = init_plugin,
};
