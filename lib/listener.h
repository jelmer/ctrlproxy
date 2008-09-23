#ifndef __CTRLPROXY_LISTENER_H__
#define __CTRLPROXY_LISTENER_H__

#include <netdb.h>
#include "ctrlproxy.h"

#ifdef HAVE_GSSAPI
#include <gssapi.h>
#endif

#ifndef G_MODULE_EXPORT
#define G_MODULE_EXPORT
#endif

struct irc_listener;

typedef void (*listener_log_fn) (enum log_level, const struct irc_listener *, const char *);

struct pending_client;

struct irc_listener_ops {
	void (*new_client) (struct pending_client *pc);
	gboolean (*handle_client_line) (struct pending_client *pc, const struct irc_line *l);
	gboolean (*socks_auth_simple) (struct pending_client *pc, const char *username, const char *password);
	gboolean (*socks_connect_ipv4) (struct pending_client *pc);
	gboolean (*socks_connect_ipv6) (struct pending_client *pc);
	gboolean (*socks_connect_fqdn) (struct pending_client *pc, const char *hostname, uint16_t port);
};

/**
 * A listener.
 */
struct irc_listener {
	int active:1;
	GIConv iconv;
	gboolean ssl;
	gpointer ssl_credentials;
	GList *incoming;
	GList *pending;
	struct listener_config *config;
	struct irc_network *network;
	struct global *global;
	listener_log_fn log_fn;
	struct irc_listener_ops *ops;
};


struct socks_method;

enum socks_state { 
	SOCKS_UNUSED = -1,
	SOCKS_UNKNOWN = 0,
	SOCKS_STATE_NEW = 1, 
	SOCKS_STATE_AUTH, 
	SOCKS_STATE_NORMAL 
};

/**
 * Client connection that has not been authenticated yet.
 */
struct pending_client {
	/** Connection to the client. */
	GIOChannel *connection;

	/** Username the client has sent. */
	char *user;
	
	/** Password the client has sent. */
	char *password;

	gint watch_id;
	struct sockaddr *clientname;
	socklen_t clientname_len;

	/** The listener used for this pending client. */
	struct irc_listener *listener;

	struct listener_iochannel *iochannel;

	/** Socks state. */
	struct {
		struct socks_method *method;
		enum socks_state state;
		void *method_data;
	} socks;

	/** Private data. */
	void *private_data;

#ifdef HAVE_GSSAPI
	gss_ctx_id_t gss_ctx;
	gss_name_t authn_name;
#endif
};

G_MODULE_EXPORT gboolean listener_start(struct irc_listener *, const char *address, const char *service);
G_MODULE_EXPORT gboolean listener_stop(struct irc_listener *);
G_MODULE_EXPORT void fini_listeners(struct global *);
G_MODULE_EXPORT void free_listener(struct irc_listener *l);
G_MODULE_EXPORT gboolean init_listeners(struct global *global);
G_MODULE_EXPORT void listener_log(enum log_level l, const struct irc_listener *listener,
				 const char *fmt, ...);
G_MODULE_EXPORT gboolean listener_socks_error(struct pending_client *pc, guint8 err);
G_MODULE_EXPORT gboolean listener_socks_reply(struct pending_client *pc, guint8 err, guint8 atyp, guint8 data_len, gchar *data, guint16 port);
G_MODULE_EXPORT struct pending_client *listener_new_pending_client(struct irc_listener *listener, GIOChannel *c);

#endif
