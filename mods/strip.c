/* 
	ctrlproxy: A modular IRC proxy
	strip: Module that removes replies to commands from other 
	clients
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

#define _GNU_SOURCE
#include "ctrlproxy.h"
#include <string.h>
#include "irc.h"

struct query_stack {
	struct query *query;
	struct network *network;
	struct client *client;	
	struct query_stack *next;
};

struct query_stack *stack = NULL;

struct query {
	char *name;
	int replies[20];
	int end_replies[20];
	/* Should add this query to the stack. return TRUE if this has 
	 * been done successfully, FALSE otherwise */
	int (*handle) (struct line *, struct query *);
};

int handle_default(struct line *, struct query *);
int handle_topic(struct line *, struct query *);

struct query queries[] = {
/* Commands that get a one-client reply: 
 * WHOIS [<server>] <nickmask>[,<nickmask>[,...]] */
	{"WHOIS", 
		{ ERR_NOSUCHSERVER, RPL_WHOISUSER, RPL_WHOISCHANNELS, RPL_AWAY,
		  RPL_WHOISIDLE, RPL_ENDOFWHOIS, ERR_NONICKNAMEGIVEN, RPL_WHOISCHANNELS,
		  RPL_WHOISSERVER, RPL_WHOISOPERATOR, ERR_NOSUCHNICK, 0 }, 
		{ ERR_NOSUCHSERVER, ERR_NONICKNAMEGIVEN, ERR_NOSUCHNICK, 
		  RPL_ENDOFWHOIS, 0 }, 
		handle_default
	},

	/* WHO [<name> [<o>]] */
	{"WHO", 
	    { ERR_NOSUCHSERVER, RPL_WHOREPLY, RPL_ENDOFWHO, 0 }, 
		{ ERR_NOSUCHSERVER, RPL_ENDOFWHO, 0 },
		handle_default 
	},

	/* NAMES [<channel>{,<channel>}]*/
	{"NAMES", 
		{ RPL_NAMREPLY, RPL_ENDOFNAMES, 0 }, 
		{ RPL_ENDOFNAMES, 0 },
		handle_default
	},

 /* LIST [<channel>{,<channel>} [<server>]*/
	{"LIST", 
		{ ERR_NOSUCHSERVER, RPL_LIST, RPL_LISTSTART, RPL_LISTEND, 0 }, 
		{ ERR_NOSUCHSERVER, RPL_LISTEND, 0 },
		handle_default
	},
	
 /* TOPIC <channel> [<topic>]*/
	{"TOPIC", 
		{ ERR_NEEDMOREPARAMS, RPL_NOTOPIC, ERR_NOTONCHANNEL, RPL_TOPIC, 
		  ERR_CHANOPRIVSNEEDED, 0 }, 
		{ ERR_NEEDMOREPARAMS, RPL_NOTOPIC, ERR_NOTONCHANNEL, RPL_TOPIC, 
		  ERR_CHANOPRIVSNEEDED, 0 }, 
		handle_topic
	},
	
 /* WHOWAS <nickname> [<count> [<server>]]*/
	{"WHOWAS", 
		{ ERR_NONICKNAMEGIVEN, ERR_WASNOSUCHNICK, RPL_WHOWASUSER,
		  RPL_WHOISSERVER, RPL_ENDOFWHOWAS, 0 },
		{ ERR_NONICKNAMEGIVEN, ERR_WASNOSUCHNICK, RPL_ENDOFWHOWAS, 
		  RPL_WHOISSERVER, 0 },
		handle_default
	},
	
 /* STATS [<query> [<server>]]*/
	{"STATS", 
		{ ERR_NOSUCHSERVER, RPL_STATSCLINE, RPL_STATSILINE, RPL_STATSQLINE, 
		  RPL_STATSLINKINFO, RPL_STATSCOMMANDS, RPL_STATSHLINE, RPL_STATSNLINE,
		  RPL_STATSKLINE, RPL_STATSLLINE, RPL_STATSUPTIME, RPL_STATSOLINE, 
		  RPL_ENDOFSTATS, 0 },
		{ ERR_NOSUCHSERVER, RPL_ENDOFSTATS, 0 },
		handle_default
	}, 
	
 /* VERSION [<server>]*/
	{"VERSION",
		{ ERR_NOSUCHSERVER, RPL_VERSION, 0 },
		{ ERR_NOSUCHSERVER, RPL_VERSION, 0 },
		handle_default
	}, 
		
 /* LINKS [[<remote server>] <server mask>]*/
	{"LINKS",
		{ ERR_NOSUCHSERVER, RPL_LINKS, RPL_ENDOFLINKS, 0 },
		{ ERR_NOSUCHSERVER, RPL_ENDOFLINKS, 0 },
		handle_default
	},
	
 /* TIME [<server>]*/
	{"TIME",
		{ ERR_NOSUCHSERVER, RPL_TIME, 0 },
		{ ERR_NOSUCHSERVER, RPL_TIME, 0 },
		handle_default
	},

 /* TRACE [<server>]*/
#if 0
	/* This one is ifdef'ed out because there is no response that 
	 * can be used as a 'end-of-response' indicator. */
	{"TRACE",
		{ ERR_NOSUCHSERVER, RPL_TRACELINK, RPL_TRACECONNECTING, 
		  RPL_TRACEUNKNOWN, RPL_TRACEUSER, RPL_TRACECLASS, 
		  RPL_TRACEHANDSHAKE, RPL_TRACEOPERATOR,
		  RPL_TRACESERVER, RPL_TRACENEWTYPE, 0 },
		{ ERR_NOSUCHSERVER, 0 },
		handle_default
	},
#endif
	
 /* SUMMON <user> [<server>]*/
	{"SUMMON",
		{ ERR_NORECIPIENT, ERR_FILEERROR, ERR_NOLOGIN, ERR_NOSUCHSERVER,
		  RPL_SUMMONING, 0 },
		{ ERR_NORECIPIENT, ERR_FILEERROR, ERR_NOLOGIN, ERR_NOSUCHSERVER,
		  RPL_SUMMONING, 0 },
		handle_default
	},
	
 /* USERS [<server>]*/
	{"USERS",
		{ ERR_NOSUCHSERVER, ERR_FILEERROR, RPL_USERSSTART, RPL_USERS,
		  RPL_NOUSERS, RPL_ENDOFUSERS, ERR_USERSDISABLED, 0 },
		{ ERR_NOSUCHSERVER, ERR_FILEERROR, ERR_USERSDISABLED, RPL_ENDOFUSERS, 0 },
		handle_default
	},
	
 /* USERHOST <nickname>{ <nickname>}{ ...}*/
	{"USERHOST",
		{ RPL_USERHOST, ERR_NEEDMOREPARAMS, 0 },
		{ RPL_USERHOST, ERR_NEEDMOREPARAMS, 0 },
		handle_default
	},
		
 /* ISON <nickname>{ <nickname>}{ ...} */
	{"ISON",
		{ RPL_ISON, ERR_NEEDMOREPARAMS, 0 },
		{ RPL_ISON, ERR_NEEDMOREPARAMS, 0 },
	    handle_default
	},

	{ NULL }
};

int is_reply(int *replies, int r)
{
	int i;
	for(i = 0; i < 20 && replies[i]; i++) {
		if(replies[i] == r) return 1;
	}
	return 0;
}

int is_numeric(char *s)
{
	while(*s) {
		if(*s < '0' || *s > '9')return 0;
		s++;
	}
	return 1;
}

struct query *find_query(char *name)
{
	int i;
	for(i = 0; queries[i].name; i++) {
		if(!strcasecmp(queries[i].name, name)) return &queries[i];
	}

	return NULL;
}

static gboolean handle_data(struct line *l) {
	struct query *q;
	
	if(l->direction == TO_SERVER) {
		q = find_query(l->args[0]);
		if(!q) return TRUE;
		/* Push it up the stack! */
		if(q->handle(l, q)) {
			/* Don't send this line to other clients */
			l->options |= LINE_IS_PRIVATE;
			return TRUE;
		}
	} else { 
		struct query_stack *s = stack, *p = NULL;
		int n;
		if(!is_numeric(l->args[0])) return TRUE;
	
		n = atoi(l->args[0]);
		
		/* Loop thru the stack until we find something that's matching */
		while(s) {
			if(is_reply(s->query->replies, n) && s->network == l->network) {
				/* Send to client that queried, if that client still exists */
				if(verify_client(s->network, s->client)) 
					irc_send_line(s->client->incoming, l);

				if(is_reply(s->query->end_replies, n)) {
					/* Remove from stack */
					if(!p)stack = s->next;	
					else p->next = s->next;
					free(s);
				}

				l->options |= LINE_DONT_SEND;

				return TRUE;
			}
			p = s; s = s->next;
		}
	}
	return TRUE;
}

gboolean fini_plugin(struct plugin *p) {
	del_filter(handle_data);
	return TRUE;
}

gboolean init_plugin(struct plugin *p) {
	add_filter("strip", handle_data);
	return TRUE;
}

int handle_default(struct line *l, struct query *q)
{
	struct query_stack *s = malloc(sizeof(struct query_stack));
	s->network = l->network;
	s->client = l->client;
	s->query = q;
	s->next = stack;
	stack = s;
	return 1;
}

int handle_topic(struct line *l, struct query *q)
{
	if(l->args[2])return 0;
	return handle_default(l,q);
}
