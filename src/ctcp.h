/* 
	ctrlproxy: A modular IRC proxy
	(c) 2002-2003 Jelmer Vernooij <jelmer@nl.linux.org>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
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
	void (*fn) (struct ctcp_handle *, char **args);
};

gboolean ctcp_process_network_request(struct network *, struct line *);
gboolean ctcp_process_network_reply(struct network *, struct line *);
gboolean ctcp_process_client_request(struct client *, struct line *);
gboolean ctcp_process_client_reply(struct client *, struct line *);
void ctcp_register_handler(const struct ctcp_handler *);
void ctcp_send(struct network *, const char *, ...);
void ctcp_reply(struct ctcp_handle *, ...);

#endif /* __CTCP_H__ */
