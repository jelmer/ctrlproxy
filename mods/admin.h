#ifndef __CTRLPROXY_ADMIN_H__
#define __CTRLPROXY_ADMIN_H__

G_MODULE_EXPORT void register_admin_command(char *cmd, void (*handler) (char **, struct line *), const char *help, const char *help_details);
G_MODULE_EXPORT void unregister_admin_command(char *cmd);
G_MODULE_EXPORT void admin_out(struct line *l, const char *fmt, ...);

#if defined(_WIN32) && !defined(ADMIN_CORE_BUILD)
#pragma comment(lib,"admin.lib")
#endif

#endif
