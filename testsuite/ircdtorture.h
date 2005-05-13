#ifndef __IRCDTORTURE_H__
#define __IRCDTORTURE_H__

void register_test(const char *name, int (*test) (void));
int new_conn(void);
int fdprintf(int fd, const char *fmt, ...);

#endif /* __IRCDTORTURE_H__ */
