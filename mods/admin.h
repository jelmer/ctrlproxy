#ifndef __CTRLPROXY_ADMIN_H__
#define __CTRLPROXY_ADMIN_H__

#include "ctrlproxy.h"

#ifndef G_MODULE_EXPORT
#define G_MODULE_EXPORT
#endif

typedef void (*admin_command_handler) (char **, struct line *, void *userdata);
G_MODULE_EXPORT void register_admin_command(char *cmd, admin_command_handler h, const char *help, const char *help_details, void *userdata);
G_MODULE_EXPORT void unregister_admin_command(char *cmd);
G_MODULE_EXPORT void admin_out(struct line *l, const char *fmt, ...);

#if defined(_WIN32) && !defined(ADMIN_CORE_BUILD)
#pragma comment(lib,"libadmin.lib")
#endif

#endif
