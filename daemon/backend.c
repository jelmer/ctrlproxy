/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2006 Jelmer Vernooij <jelmer@nl.linux.org>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <pwd.h>
#include "internals.h"
#include "daemon/backend.h"
#include "irc.h"
#include <sys/un.h>

static gboolean daemon_backend_recv(struct irc_transport *transport, const struct irc_line *line)
{
	struct daemon_backend *backend = transport->userdata;

	if (!backend->authenticated) {
		gboolean ok;
		if (atoi(line->args[0]) == ERR_PASSWDMISMATCH) {
			ok = FALSE;
		} else if (!strcmp(line->args[0], "NOTICE") && 
				   !strcmp(line->args[1], "AUTH") && 
				   !strcmp(line->args[2], "PASS OK")) {
			ok = TRUE;
		} else {
			char *msg = g_strdup_printf("Unexpected response %s", line->args[0]);
			if (!backend->callbacks->error(backend, msg))
				return FALSE;
			g_free(msg);
			ok = FALSE;
		}
		if (backend->pass_check_cb != NULL) {
			backend->pass_check_cb(backend, ok);
			backend->pass_check_cb = NULL;
		}
		backend->authenticated = ok;
		return TRUE;
	} else {
		return backend->callbacks->recv(backend, line);
	}
}

static void on_daemon_backend_disconnect(struct irc_transport *transport)
{
	struct daemon_backend *backend = transport->userdata;

	backend->callbacks->disconnect(backend);
}

static void charset_error_not_called(struct irc_transport *transport, const char *error_msg)
{
	g_assert_not_reached();
}

static const struct irc_transport_callbacks daemon_backend_callbacks = {
	.hangup = irc_transport_disconnect,
	.log = NULL,
	.disconnect = on_daemon_backend_disconnect,
	.recv = daemon_backend_recv,
	.charset_error = charset_error_not_called,
	.error = NULL,
};

struct daemon_backend *daemon_backend_open(const char *socketpath,
											const struct daemon_backend_callbacks *callbacks,
											gpointer userdata,
											struct irc_listener *listener)
{
	struct daemon_backend *backend;
	int sock;
	struct sockaddr_un un;
	GIOChannel *ch;

	sock = socket(PF_UNIX, SOCK_STREAM, 0);

	un.sun_family = AF_UNIX;
	strncpy(un.sun_path, socketpath, sizeof(un.sun_path));

	if (connect(sock, (struct sockaddr *)&un, sizeof(un)) < 0) {
		listener_log(LOG_INFO, listener, "unable to connect to %s: %s", un.sun_path, strerror(errno));
		close(sock);
		return NULL;
	}

	backend = g_new0(struct daemon_backend, 1);
	ch = g_io_channel_unix_new(sock);

	g_io_channel_set_encoding(ch, NULL, NULL);
	g_io_channel_set_flags(ch, G_IO_FLAG_NONBLOCK, NULL);

	backend->transport = irc_transport_new_iochannel(ch);
	if (backend->transport == NULL) {
		return FALSE;
	}

	irc_transport_set_callbacks(backend->transport, &daemon_backend_callbacks, backend);

	backend->userdata = userdata;
	backend->callbacks = callbacks;

	return backend;
}

gboolean daemon_backend_authenticate(struct daemon_backend *backend,
									 const char *password,
									 void (*callback) (struct daemon_backend *backend, gboolean))
{
	g_assert(!backend->authenticated);
	g_assert(backend->pass_check_cb == NULL);
	backend->pass_check_cb = callback;

	backend->authenticated = FALSE;

	if (!transport_send_args(backend->transport, "PASS", password, NULL))
		return FALSE;

	/* FIXME: Register timeout and raise error if backend didn't respond in time */

	return TRUE;
}

void daemon_backend_kill(struct daemon_backend *backend)
{
	irc_transport_disconnect(backend->transport);
	free_irc_transport(backend->transport);
	backend->transport = NULL;
}

gboolean daemon_backend_send_line (struct daemon_backend *backend, const struct irc_line *line)
{
	return transport_send_line(backend->transport, line);
}
