/* 
	ctrlproxy: A modular IRC proxy
	(c) 2002-2003 Jelmer Vernooij <jelmer@nl.linux.org>

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
	char *name;
	void (*fn) (struct ctcp_handle *, char **args);
};

gboolean ctcp_process_network_request(struct irc_network *, const struct irc_line *);
gboolean ctcp_process_network_reply(struct irc_network *, const struct irc_line *);
gboolean ctcp_process_client_request(struct irc_client *, struct irc_line *);
gboolean ctcp_process_client_reply(struct irc_client *, struct irc_line *);
G_MODULE_EXPORT void ctcp_register_handler(const struct ctcp_handler *);
G_MODULE_EXPORT G_GNUC_NULL_TERMINATED void ctcp_send(struct irc_network *, const char *, ...);
G_MODULE_EXPORT G_GNUC_NULL_TERMINATED void ctcp_reply(struct ctcp_handle *, ...);

#endif /* __CTCP_H__ */
