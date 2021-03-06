/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2003 Jelmer Vernooĳ <jelmer@jelmer.uk>

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

#ifndef __CTCP_H__
#define __CTCP_H__

/**
 * @file
 * @brief CTCP handling
 */
struct ctcp_handle;

/**
 * CTCP command handling
 */
struct ctcp_handler {
	const char *name;
	void (*fn) (struct ctcp_handle *, const char **args);
};

gboolean ctcp_network_redirect_response(struct irc_network *, const struct irc_line *);
gboolean ctcp_client_request_record(struct irc_client *, struct irc_line *);
gboolean ctcp_process_request(struct irc_network *, const struct irc_line *);
G_MODULE_EXPORT void ctcp_register_handler(const struct ctcp_handler *);
G_MODULE_EXPORT G_GNUC_NULL_TERMINATED void ctcp_send(struct irc_network *, const char *, ...);
G_MODULE_EXPORT G_GNUC_NULL_TERMINATED void ctcp_reply(struct ctcp_handle *, ...);
struct irc_network *ctcp_get_network(struct ctcp_handle *h);
struct network_nick *ctcp_get_network_nick(struct ctcp_handle *h);
const char *ctcp_get_nick(struct ctcp_handle *h);

#endif /* __CTCP_H__ */
