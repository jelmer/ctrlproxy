#ifndef __TORTURE_H__
#define __TORTURE_H__

#include <glib.h>

void register_test(const char *name, int (*test) (void));

#endif /* __TORTURE_H__ */
