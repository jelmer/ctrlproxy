#ifndef _TORTURE_H_
#define _TORTURE_H_

/**
 * @file
 * @brief Testsuite utility functions.
 */

gboolean g_io_channel_pair(GIOChannel **ch1, GIOChannel **ch2);
struct global *torture_global(const char *name);
char *torture_tempfile(const char *path);
struct network *dummy_network(void);
#define TORTURE_GLOBAL torture_global(__FUNCTION__)

#endif /* _TORTURE_H_ */
