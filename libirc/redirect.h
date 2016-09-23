/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2005 Jelmer Vernooĳ <jelmer@jelmer.uk>

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

/**
 * IRC Query done by a client
 */
struct query {
	/** Command name. */
	char *name;

	/** Possible replies */
	int replies[20];
	/** Possible replies that are the last reply to this command. */
	int end_replies[20];
	/** Possible errors. */
	int errors[20];
	/* Should add this query to the stack. return TRUE if this has
	 * been done successfully, FALSE otherwise */
	/** Function for handling the responses. */
	int (*handle) (const struct irc_line *, struct query_stack *, void *userdata, struct query *);
};



struct query_stack_entry {
	const struct query *query;
	void *userdata;
	time_t time;
};

struct query_stack {
	GList *entries;
	void (*unref_userdata) (void *);
	void (*ref_userdata) (void *);
};



void *query_stack_match_response(struct query_stack *stack, const struct irc_line *l);
gboolean query_stack_record(struct query_stack *stack, void *c, const struct irc_line *l);
G_GNUC_WARN_UNUSED_RESULT struct query_stack *new_query_stack(void (*ref_userdata) (void *), void (*unref_userdata) (void *));
void query_stack_clear(struct query_stack *n);
void query_stack_free(struct query_stack *n);


#endif /* __REDIRECT_H__ */
