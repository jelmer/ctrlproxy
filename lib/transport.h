/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2008 Jelmer Vernooij <jelmer@nl.linux.org>

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

#ifndef __LIBIRC_TRANSPORT_H__
#define __LIBIRC_TRANSPORT_H__

#include <glib.h>
#include "line.h"

/**
 * @file
 * @brief Transport functions
 */

struct irc_transport;

typedef void (*transport_log_fn)(struct irc_transport *transport, const struct irc_line *l, const GError *error);
typedef void (*transport_disconnect_fn)(struct irc_transport *transport);
typedef gboolean (*transport_recv_fn) (struct irc_transport *transport, const struct irc_line *line);

struct irc_transport {
	GIOChannel *incoming;
	gint incoming_id;
	gint outgoing_id;
	GIConv incoming_iconv;
	GIConv outgoing_iconv;
	GQueue *pending_lines;
	char *charset;
	transport_log_fn log_fn;
	transport_disconnect_fn disconnect_fn;
	transport_recv_fn recv_fn;
	void *userdata;
};

struct irc_transport *irc_transport_new_iochannel(GIOChannel *iochannel,
												  transport_log_fn log_fn,
												  transport_disconnect_fn disconnect_fn,
												  transport_recv_fn recv_fn,
												  void *userdata);
void irc_transport_disconnect(struct irc_transport *transport);
void free_irc_transport(struct irc_transport *);
gboolean transport_set_charset(struct irc_transport *transport, const char *name);
gboolean transport_send_line(struct irc_transport *transport, const struct irc_line *);

#endif /* __LIBIRC_TRANSPORT_H__ */
