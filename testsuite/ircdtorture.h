#ifndef __IRCDTORTURE_H__
#define __IRCDTORTURE_H__

#include <glib.h>
#include "../ctrlproxy.h"

void register_test(const char *name, int (*test) (void));
GIOChannel *new_conn(void);

#endif /* __IRCDTORTURE_H__ */
