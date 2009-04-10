/* 
	ctrlproxy: A modular IRC proxy
	(c) 2002-2005 Jelmer Vernooij <jelmer@nl.linux.org>

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

#ifndef __REDIRECT_H__
#define __REDIRECT_H__

struct query_stack;

void redirect_record(struct query_stack **stack,
					 const struct irc_network *n, struct irc_client *c, 
					 const struct irc_line *l);
gboolean redirect_response(struct query_stack **stack, 
						   struct irc_network *network,
						   const struct irc_line *l);
void redirect_clear(struct query_stack **n);

#endif /* __REDIRECT_H__ */
