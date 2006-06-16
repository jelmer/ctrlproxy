#ifndef __CTRLPROXY_ADMIN_H__
#define __CTRLPROXY_ADMIN_H__

#include "client.h"

typedef void (*admin_command_handler) (struct client *c, char **, void *userdata);
struct admin_command {
	char *name;
	admin_command_handler handler;
	const char *help;
	const char *help_details;
	void *userdata;
};

G_MODULE_EXPORT void register_admin_command(const struct admin_command *cmd);
G_MODULE_EXPORT void unregister_admin_command(const struct admin_command *cmd);
G_MODULE_EXPORT void admin_out(struct client *c, const char *fmt, ...);

#endif
