
#ifndef _CTRLPROXY_CACHE_H_
#define _CTRLPROXY_CACHE_H_

struct irc_line;
struct irc_network_state;
struct irc_client;

/* cache.c */
struct cache_settings {
	int max_who_age;
};
gboolean client_try_cache(struct irc_client *c, struct irc_network_state *n, struct irc_line *l, const struct cache_settings *settings);

#endif

