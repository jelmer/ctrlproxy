#include <glib.h>
#include <stdlib.h>

void log_handler(const gchar *log_domain, GLogLevelFlags flags, const gchar *message, gpointer user_data);
	


void register_log_function() {
		g_log_set_handler(NULL, G_LOG_LEVEL_MASK | G_LOG_FLAG_RECURSION, log_handler, NULL);
}

