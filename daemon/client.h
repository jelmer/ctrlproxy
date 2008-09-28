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

#ifndef HAVE_DAEMON_CLIENT_H
#define HAVE_DAEMON_CLIENT_H

struct daemon_client {
	struct irc_transport *client_transport;
	struct irc_listener *listener;
	struct daemon_user *user;
	struct daemon_backend *backend;
	struct ctrlproxyd_config *config;
	char *password;
	char *servername;
	char *servicename;
	char *nick;
	char *username;
	char *mode;
	char *unused;
	char *realname;
	char *description;
	gboolean freed;
	gboolean inetd;
};

void daemon_client_kill(struct daemon_client *dc);
void daemon_client_send_credentials(struct daemon_client *dc);
void daemon_clients_exit(void);

#endif
