#ifndef __CTRLPROXY_ADMIN_H__
#define __CTRLPROXY_ADMIN_H__

void register_admin_command(char *cmd, void (*handler) (char **, struct line *), char *help, char *help_details);
void unregister_admin_command(char *cmd);
void admin_out(struct line *l, char *fmt, ...);

#endif
