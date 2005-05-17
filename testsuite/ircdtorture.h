#ifndef __IRCDTORTURE_H__
#define __IRCDTORTURE_H__

#include <glib.h>
#include "../ctrlproxy.h"

void register_test(const char *name, int (*test) (void));
GIOChannel *new_conn(void);
GIOChannel *new_conn_loggedin(const char *nick);
struct line *wait_response(GIOChannel *, const char *cmd);
struct line *wait_responses(GIOChannel *, const char *cmd[]);

#endif /* __IRCDTORTURE_H__ */
