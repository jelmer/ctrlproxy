/* 
	ctrlproxy: A modular IRC proxy
	mux: Send numerics to the right places
	(c) 2002-2005 Jelmer Vernooij <jelmer@nl.linux.org>

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

#include "ctrlproxy.h"
#include <string.h>
#include "irc.h"

struct query_stack {
	struct query *query;
	struct network *network;
	struct client *client;	
	struct query_stack *next;
};

static struct query_stack *stack = NULL;

struct query {
	char *name;
	int replies[20];
	int end_replies[20];
	/* Should add this query to the stack. return TRUE if this has 
	 * been done successfully, FALSE otherwise */
	int (*handle) (struct line *, struct query *);
};

static int handle_default(struct line *, struct query *);
static int handle_topic(struct line *, struct query *);

static struct query queries[] = {
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
		{ ERR_TOOMANYMATCHES, ERR_NOSUCHSERVER, RPL_NAMREPLY, RPL_ENDOFNAMES, 0 }, 
		{ ERR_TOOMANYMATCHES, ERR_NOSUCHSERVER, RPL_ENDOFNAMES, 0 },
		handle_default
	},

 /* LIST [<channel>{,<channel>} [<server>]*/
	{"LIST", 
		{ ERR_TOOMANYMATCHES, ERR_NOSUCHSERVER, RPL_LIST, RPL_LISTSTART, RPL_LISTEND, 0 }, 
		{ ERR_TOOMANYMATCHES, ERR_NOSUCHSERVER, RPL_LISTEND, 0 },
		handle_default
	},
	
 /* TOPIC <channel> [<topic>]*/
	{"TOPIC", 
		{ ERR_NEEDMOREPARAMS, RPL_NOTOPIC, ERR_NOTONCHANNEL, RPL_TOPIC, 
		  ERR_CHANOPPRIVSNEEDED, ERR_NOCHANMODES, 0 }, 
		{ ERR_NEEDMOREPARAMS, RPL_NOTOPIC, ERR_NOTONCHANNEL, RPL_TOPIC, 
		  ERR_CHANOPPRIVSNEEDED, ERR_NOCHANMODES, 0 }, 
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
	{"TRACE",
		{ ERR_NOSUCHSERVER, RPL_TRACELINK, RPL_TRACECONNECTING, 
		  RPL_TRACEUNKNOWN, RPL_TRACEUSER, RPL_TRACECLASS, 
		  RPL_TRACEHANDSHAKE, RPL_TRACEOPERATOR,
		  RPL_TRACESERVER, RPL_TRACENEWTYPE, 0 },
		{ ERR_NOSUCHSERVER, 0 },
		handle_default
	},
	
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

/* JOIN <channel>{,<channel>} [<key>{,<key>}] */
	{"JOIN",
		{ ERR_NEEDMOREPARAMS, ERR_BANNEDFROMCHAN,
		  ERR_INVITEONLYCHAN, ERR_BADCHANNELKEY,
		  ERR_CHANNELISFULL, ERR_BADCHANMASK,
		  ERR_NOSUCHCHANNEL, ERR_TOOMANYCHANNELS,
		  RPL_TOPIC, 0 },
		{ ERR_NEEDMOREPARAMS, ERR_BANNEDFROMCHAN,
		  ERR_INVITEONLYCHAN, ERR_BADCHANNELKEY,
		  ERR_CHANNELISFULL, ERR_BADCHANMASK,
		  ERR_NOSUCHCHANNEL, ERR_TOOMANYCHANNELS,
		  RPL_TOPIC, 0 },
		handle_default
	},

 /* PART <channel> *( "," <channel> ) [ <Part Message> ] */
	{ "PART",
		{ ERR_NEEDMOREPARAMS, ERR_NOSUCHCHANNEL, ERR_NOTONCHANNEL, 0 },
		{ ERR_NEEDMOREPARAMS, ERR_NOSUCHCHANNEL, ERR_NOTONCHANNEL, 0 },
		handle_default
	},

 /* NICK <nickname> */
	{ "NICK",
		{ ERR_NONICKNAMEGIVEN, ERR_ERRONEUSNICKNAME, ERR_NICKNAMEINUSE,
	      ERR_UNAVAILRESOURCE, ERR_RESTRICTED, ERR_NICKCOLLISION, 0 },
		{ ERR_NONICKNAMEGIVEN, ERR_ERRONEUSNICKNAME, ERR_NICKNAMEINUSE,
	      ERR_UNAVAILRESOURCE, ERR_RESTRICTED, ERR_NICKCOLLISION, 0 },
		handle_default
	},

 /* OPER <name> <password> */
	{ "OPER",
	    { ERR_NEEDMOREPARAMS, RPL_YOUREOPER, 
			ERR_NOOPERHOST, ERR_PASSWDMISMATCH, 0 },
	    { ERR_NEEDMOREPARAMS, RPL_YOUREOPER, 
			ERR_NOOPERHOST, ERR_PASSWDMISMATCH, 0 },
		handle_default
	},
	
 /* MODE <nick> <mode> */
	{ "MODE", 
		/* FIXME: Channel modes */
		{ ERR_NEEDMOREPARAMS, ERR_UMODEUNKNOWNFLAG, 
			RPL_UMODEIS, ERR_USERSDONTMATCH, 0 },
		{ ERR_NEEDMOREPARAMS, ERR_UMODEUNKNOWNFLAG, 
			RPL_UMODEIS, ERR_USERSDONTMATCH, 0 },
		handle_default
	},
	
 /* SERVICE <nick> <reserved> <distribution> <type> <reserved> <info> */
	{ "SERVICE", 
		{ ERR_ALREADYREGISTERED, ERR_NEEDMOREPARAMS, ERR_ERRONEUSNICKNAME,
			RPL_YOURESERVICE, RPL_YOURHOST, RPL_MYINFO, 0 },
		{ ERR_ALREADYREGISTERED, ERR_NEEDMOREPARAMS, ERR_ERRONEUSNICKNAME,
			RPL_YOURESERVICE, RPL_YOURHOST, RPL_MYINFO, 0 },
		handle_default
	},
	
 /* SQUIT <server> <comment> */
	{ "SQUIT",
		{ ERR_NOPRIVILEGES, ERR_NEEDMOREPARAMS, ERR_NOSUCHSERVER, 0 },
		{ ERR_NOPRIVILEGES, ERR_NEEDMOREPARAMS, ERR_NOSUCHSERVER, 0 },
		handle_default
	},
	
 /* INVITE <nick> <channel> */
	{ "INVITE", 
		{ ERR_NEEDMOREPARAMS, ERR_NOTONCHANNEL, ERR_NOSUCHNICK, 
			ERR_CHANOPPRIVSNEEDED, ERR_USERONCHANNEL, RPL_INVITING, RPL_AWAY, 0 },
		{ ERR_NEEDMOREPARAMS, ERR_NOTONCHANNEL, ERR_NOSUCHNICK, 
			ERR_CHANOPPRIVSNEEDED, ERR_USERONCHANNEL, RPL_INVITING, RPL_AWAY, 0 },
		handle_default
	},
	
/* KICK <channel> * ( "," <channel> ) <user> *( "," <user> ) [<comment>] */
	{ "KICK",
		{ ERR_NEEDMOREPARAMS, ERR_BADCHANMASK, ERR_USERNOTINCHANNEL,
			ERR_NOSUCHCHANNEL, ERR_CHANOPPRIVSNEEDED, ERR_NOTONCHANNEL, 0 },
		{ ERR_NEEDMOREPARAMS, ERR_BADCHANMASK, ERR_USERNOTINCHANNEL,
			ERR_NOSUCHCHANNEL, ERR_CHANOPPRIVSNEEDED, ERR_NOTONCHANNEL, 0 },
		handle_default
	},
 
 /* PRIVMSG <msgtarget> <text> */
	{ "PRIVMSG",
		{ ERR_NORECIPIENT, ERR_NOTEXTTOSEND, ERR_CANNOTSENDTOCHAN, 
			ERR_NOTOPLEVEL, ERR_TOOMANYTARGETS, ERR_WILDTOPLEVEL, 
			ERR_NOSUCHNICK, RPL_AWAY, 0 },
		{ ERR_NORECIPIENT, ERR_NOTEXTTOSEND, ERR_CANNOTSENDTOCHAN, 
			ERR_NOTOPLEVEL, ERR_TOOMANYTARGETS, ERR_WILDTOPLEVEL, 
			ERR_NOSUCHNICK, RPL_AWAY, 0 },
		handle_default
	},
 
 /* MOTD [<target>] */
	{ "MOTD",
		{ RPL_MOTDSTART, RPL_MOTD, ERR_NOMOTD, RPL_ENDOFMOTD, 0 },
		{ ERR_NOMOTD, RPL_ENDOFMOTD, 0 },
		handle_default
	},

 /* LUSERS [ <mask> [ <target> ] ] */
	{ "LUSERS",
		{ RPL_LUSERCLIENT, RPL_LUSEROP, RPL_LUSERUNKNOWN, RPL_LUSERCHANNELS, 
			RPL_LUSERME, ERR_NOSUCHSERVER, 0 },
		{ ERR_NOSUCHSERVER, 0 },
		handle_default
	},
	
 /* CONNECT <target> <port> [ <remote server> ] */
	{ "CONNECT",
		{ ERR_NOSUCHSERVER, ERR_NEEDMOREPARAMS, ERR_NOPRIVILEGES, 0 },
		{ ERR_NOSUCHSERVER, ERR_NEEDMOREPARAMS, ERR_NOPRIVILEGES, 0 },
		handle_default
	},

 /* ADMIN [ <target> ] */
	{ "ADMIN",
		{ ERR_NOSUCHSERVER, RPL_ADMINME, RPL_ADMINLOC2, RPL_ADMINLOC1, 
			RPL_ADMINLOC3, 0 },
		{ ERR_NOSUCHSERVER, 0 },
		handle_default
	},
	
 /* INFO [ <target> ] */
	{ "INFO",
		{ ERR_NOSUCHSERVER, RPL_INFO, RPL_ENDOFINFO, 0 },
		{ ERR_NOSUCHSERVER, RPL_ENDOFINFO, 0 },
		handle_default
	},
	
 /* SERVLIST [ <mask> [ <type> ] ] */
	{ "SERVLIST",
		{ RPL_SERVLIST, RPL_SERVLISTEND, 0},
		{ RPL_SERVLISTEND, 0 },
		handle_default
	},
	
 /* SQUERY <servicename> <text> */
	/* Same responses as for PRIVMSG */
	{ "SQUERY",
		{ ERR_NORECIPIENT, ERR_NOTEXTTOSEND, ERR_CANNOTSENDTOCHAN, 
			ERR_NOTOPLEVEL, ERR_TOOMANYTARGETS, ERR_WILDTOPLEVEL, 
			ERR_NOSUCHNICK, RPL_AWAY, 0 },
		{ ERR_NORECIPIENT, ERR_NOTEXTTOSEND, ERR_CANNOTSENDTOCHAN, 
			ERR_NOTOPLEVEL, ERR_TOOMANYTARGETS, ERR_WILDTOPLEVEL, 
			ERR_NOSUCHNICK, RPL_AWAY, 0 },
		handle_default
	},
	
 /* KILL <nick> <comment> */
	{ "KILL",
		{ ERR_NOPRIVILEGES, ERR_NEEDMOREPARAMS, ERR_NOSUCHNICK, ERR_CANTKILLSERVER, 0 },
		{ ERR_NOPRIVILEGES, ERR_NEEDMOREPARAMS, ERR_NOSUCHNICK, ERR_CANTKILLSERVER, 0 },
		handle_default
	},

 /* AWAY [ <text> ] */
	{ "AWAY",
		{ RPL_UNAWAY, RPL_NOWAWAY, 0 },
		{ RPL_UNAWAY, RPL_NOWAWAY, 0 },
		handle_default
	},

 /* REHASH */
	{ "REHASH",
		{ RPL_REHASHING, ERR_NOPRIVILEGES, 0 },
		{ RPL_REHASHING, ERR_NOPRIVILEGES, 0 },
		handle_default
	},

 /* DIE */
	{ "DIE",
		{ ERR_NOPRIVILEGES, 0 },
		{ ERR_NOPRIVILEGES, 0 },
		handle_default
	},

 /* RESTART */
	{ "RESTART",
		{ ERR_NOPRIVILEGES, 0 },
		{ ERR_NOPRIVILEGES, 0 },
		handle_default
	},

 /* WALLOPS */
	{ "WALLOPS",
		{ ERR_NEEDMOREPARAMS, 0 },
		{ ERR_NEEDMOREPARAMS, 0 },
		handle_default
	},

	{ NULL }
};

/* List of responses that should be sent to all clients */
static int response_all[] = { RPL_NOWAWAY, RPL_UNAWAY, 0 };

static int is_reply(int *replies, int r)
{
	int i;
	for(i = 0; i < 20 && replies[i]; i++) {
		if(replies[i] == r) return 1;
	}
	return 0;
}

static struct query *find_query(char *name)
{
	int i;
	for(i = 0; queries[i].name; i++) {
		if(!g_strcasecmp(queries[i].name, name)) return &queries[i];
	}

	return NULL;
}

void redirect_response(struct network *network, struct line *l)
{
	struct query_stack *s = stack, *p = NULL;
	struct client *c = NULL;
	int n;
	int i;

	n = atoi(l->args[0]);

	/* Find a request that this response is a reply to */
	while(s) {
		if(is_reply(s->query->replies, n) && s->network == l->network) {
			/* Send to client that queried, if that client still exists */
			if(verify_client(s->network, s->client)) {
				c = s->client;
				client_send_line(s->client, l);
			}

			if(is_reply(s->query->end_replies, n)) {
				/* Remove from stack */
				if(!p)stack = s->next;	
				else p->next = s->next;
				g_free(s);
			}

			break;
		}
		p = s; s = s->next;
	}

	/* See if this is a response that should be sent to all clients */
	for (i = 0; response_all[i]; i++) {
		if (response_all[i] == n) {
			clients_send(network, l, c);
			return;
		}
	}

	if (!c) {
		log_network(NULL, network, "Unable to redirect response %s", l->args[0]);
		clients_send(network, l, NULL);
	}
}

void redirect_record(struct client *c, struct line *l)
{
	struct query *q;

	q = find_query(l->args[0]);
	if(!q) return;

	/* Push it up the stack! */
	q->handle(l, q);
}

static int handle_default(struct line *l, struct query *q)
{
	struct query_stack *s = g_new(struct query_stack,1);
	s->network = l->network;
	s->client = l->client;
	s->query = q;
	s->next = stack;
	stack = s;
	return 1;
}

static int handle_topic(struct line *l, struct query *q)
{
	if(l->args[2])return 0;
	return handle_default(l,q);
}
