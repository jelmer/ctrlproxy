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

#ifndef HAVE_DAEMON_BACKEND_H
#define HAVE_DAEMON_BACKEND_H

struct daemon_backend;
struct daemon_user;
struct daemon_backend_callbacks;

struct daemon_backend {
	struct irc_transport *transport;
	gboolean authenticated;
	void (*pass_check_cb) (struct daemon_backend *backend, gboolean accepted);
	gpointer pass_check_data;
	gpointer userdata;
	const struct daemon_backend_callbacks *callbacks;
};

struct daemon_backend_callbacks {
	void (*disconnect) (struct daemon_backend *);
	gboolean (*error) (struct daemon_backend *, const char *error_msg);
	gboolean (*recv) (struct daemon_backend *, const struct irc_line *line);
};

struct daemon_backend *daemon_backend_open(const char *socketpath,
											   const struct daemon_backend_callbacks *callbacks,
											   gpointer userdata);
gboolean daemon_backend_authenticate(struct daemon_backend *backend,
									 const char *password,
									 void (*callback) (struct daemon_backend *backend, gboolean));
void daemon_backend_kill(struct daemon_backend *backend);
gboolean daemon_backend_send_line(struct daemon_backend *backend, const struct irc_line *line);

#endif
