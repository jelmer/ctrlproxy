/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2000-2003, Ximian, Inc.
 */

#ifndef SSL_H
#define SSL_H 1

#include <glib.h>

/**
 * SSLType:
 * @SSL_TYPE_CLIENT: the client side of an SSL connection
 * @SSL_TYPE_SERVER: the server side of an SSL connection
 *
 * What kind of SSL connection this is.
 **/
typedef enum {
	SSL_TYPE_CLIENT = 0,
	SSL_TYPE_SERVER
} SSLType;

gpointer    ssl_get_client_credentials  (const char  *ca_file);
void        ssl_free_client_credentials (gpointer     creds);
gpointer    ssl_get_server_credentials  (const char  *cert_file,
					      const char  *key_file);
void        ssl_free_server_credentials (gpointer     creds);

GIOChannel *ssl_wrap_iochannel          (GIOChannel  *sock,
					      SSLType  type,
					      const char  *remote_host,
					      gpointer     credentials);

#define SSL_ERROR ssl_error_quark()

GQuark ssl_error_quark (void);

typedef enum {
	SSL_ERROR_HANDSHAKE_NEEDS_READ,
	SSL_ERROR_HANDSHAKE_NEEDS_WRITE,
	SSL_ERROR_CERTIFICATE,
} SocketError;

void ssl_cert_generate(const char *keyfile, const char *certfile,
		       const char *cafile);

gpointer ssl_create_server_credentials(struct global *global, 
									   GKeyFile *kf, const char *group);

#endif /* SSL_H */
