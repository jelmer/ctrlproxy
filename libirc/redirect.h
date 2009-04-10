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

void *query_stack_match_response(struct query_stack *stack, const struct irc_line *l);
gboolean query_stack_record(struct query_stack *stack, void *c, const struct irc_line *l);
struct query_stack *new_query_stack(void (*ref_userdata) (void *), void (*unref_userdata) (void *));
void query_stack_clear(struct query_stack *n);
void query_stack_free(struct query_stack *n);


#endif /* __REDIRECT_H__ */
