#ifndef _TORTURE_H_
#define _TORTURE_H_

gboolean g_io_channel_pair(GIOChannel **ch1, GIOChannel **ch2);
struct global *torture_global(const char *name);
#define TORTURE_GLOBAL torture_global(__FUNCTION__)

#endif /* _TORTURE_H_ */
