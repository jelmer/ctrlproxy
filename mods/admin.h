#ifndef __CTRLPROXY_ADMIN_H__
#define __CTRLPROXY_ADMIN_H__

void register_admin_command(char *cmd, void (*handler) (char **, struct line *));
void unregister_admin_command(char *cmd);

#endif
