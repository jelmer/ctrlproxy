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

struct irc_transport_callbacks {
	void (*log)(struct irc_transport *transport, const struct irc_line *l, const GError *error);
	void (*hangup) (struct irc_transport *transport);
	void (*disconnect)(struct irc_transport *transport);
	gboolean (*recv)(struct irc_transport *transport, const struct irc_line *line);
	void (*charset_error) (struct irc_transport *transport, const char *error_msg);
	gboolean (*error) (struct irc_transport *transport, const char *error_msg);
};

struct irc_transport {
	GIOChannel *incoming;
	gint incoming_id;
	gint outgoing_id;
	GIConv incoming_iconv;
	GIConv outgoing_iconv;
	GQueue *pending_lines;
	char *charset;
	const struct irc_transport_callbacks *callbacks;
	void *userdata;
	time_t last_line_sent;
};

struct irc_transport *irc_transport_new_iochannel(GIOChannel *iochannel);
void irc_transport_disconnect(struct irc_transport *transport);
void free_irc_transport(struct irc_transport *);
gboolean transport_set_charset(struct irc_transport *transport, const char *name);
gboolean transport_send_line(struct irc_transport *transport, const struct irc_line *);
gboolean transport_send_args(struct irc_transport *transport, ...);
void transport_parse_buffer(struct irc_transport *transport);
void irc_transport_set_callbacks(struct irc_transport *transport, 
								 const struct irc_transport_callbacks *callbacks, void *userdata);

#endif /* __LIBIRC_TRANSPORT_H__ */
