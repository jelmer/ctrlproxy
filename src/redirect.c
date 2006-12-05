/* 
	ctrlproxy: A modular IRC proxy
	Send numerics to the right places
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

/* TODO: Clean up stack occasionally */

struct query_stack {
	const struct query *query;
	const struct network *network;
	struct client *client;	
	time_t time;
	struct query_stack *next;
};

static struct query_stack *stack = NULL;

/**
 * IRC Query done by a client
 */
struct query {
	char *name;
	int replies[20];
	int end_replies[20];
	int errors[20];
	/* Should add this query to the stack. return TRUE if this has 
	 * been done successfully, FALSE otherwise */
	int (*handle) (const struct line *, const struct network *n, struct client *c, struct query *);
};

static int handle_default(const struct line *, const struct network *n, struct client *c, struct query *);
static int handle_topic(const struct line *, const struct network *n, struct client *c, struct query *);

static struct query queries[] = {
/* Commands that get a one-client reply: 
 * WHOIS [<server>] <nickmask>[,<nickmask>[,...]] */
	{"WHOIS", 
		{ RPL_WHOISUSER, RPL_WHOISCHANNELS, RPL_AWAY,
		  RPL_WHOISIDLE, RPL_WHOISCHANNELS,
		  RPL_WHOISSERVER, RPL_WHOISOPERATOR, 
		  RPL_WHOISIDENTIFIED, 0 }, 
		{ RPL_ENDOFWHOIS, 0 }, 
		{ ERR_NOSUCHSERVER, ERR_NONICKNAMEGIVEN, ERR_NOSUCHNICK, 0 },
		handle_default
	},

	/* WHO [<name> [<o>]] */
	{"WHO", 
	    { RPL_WHOREPLY, 0 }, 
		{ RPL_ENDOFWHO, 0 },
		{ ERR_NOSUCHSERVER, 0 },
		handle_default 
	},

	/* NAMES [<channel>{,<channel>}]*/
	{"NAMES", 
		{ RPL_NAMREPLY, 0 }, 
		{ RPL_ENDOFNAMES, 0 },
		{ ERR_TOOMANYMATCHES, ERR_NOSUCHSERVER, 0 },
		handle_default
	},

 /* LIST [<channel>{,<channel>} [<server>]*/
	{"LIST", 
		{ RPL_LIST, RPL_LISTSTART, 0 }, 
		{ RPL_LISTEND, 0 },
		{ ERR_TOOMANYMATCHES, ERR_NOSUCHSERVER, 0 },
		handle_default
	},
	
 /* TOPIC <channel> [<topic>]*/
	{"TOPIC", 
		{ 0 }, 
		{ RPL_NOTOPIC, RPL_TOPIC, 0 }, 
		{ ERR_NOTONCHANNEL, ERR_NEEDMOREPARAMS, ERR_CHANOPPRIVSNEEDED, 
			ERR_NOCHANMODES, 0 },
		handle_topic
	},
	
 /* WHOWAS <nickname> [<count> [<server>]]*/
	{"WHOWAS", 
		{ RPL_WHOWASUSER, 0 },
		{ RPL_ENDOFWHOWAS, RPL_WHOISSERVER, 0 },
		{ ERR_NONICKNAMEGIVEN, ERR_WASNOSUCHNICK, 0 },
		handle_default
	},
	
 /* STATS [<query> [<server>]]*/
	{"STATS", 
		{ RPL_STATSCLINE, RPL_STATSILINE, RPL_STATSQLINE, 
		  RPL_STATSLINKINFO, RPL_STATSCOMMANDS, RPL_STATSHLINE, RPL_STATSNLINE,
		  RPL_STATSKLINE, RPL_STATSLLINE, RPL_STATSUPTIME, RPL_STATSOLINE, 
		  0 },
		{ RPL_ENDOFSTATS, 0 },
		{ ERR_NOSUCHSERVER, 0 },
		handle_default
	}, 
	
 /* VERSION [<server>]*/
	{"VERSION",
		{ 0 },
		{ RPL_VERSION, 0 },
		{ ERR_NOSUCHSERVER, 0 },
		handle_default
	}, 
		
 /* LINKS [[<remote server>] <server mask>]*/
	{"LINKS",
		{ RPL_LINKS, 0 },
		{ RPL_ENDOFLINKS, 0 },
		{ ERR_NOSUCHSERVER, 0 },
		handle_default
	},
	
 /* TIME [<server>]*/
	{"TIME",
		{ 0 },
		{ RPL_TIME, 0 },
		{ ERR_NOSUCHSERVER, 0 },
		handle_default
	},

 /* TRACE [<server>]*/
	{"TRACE",
		{ RPL_TRACELINK, RPL_TRACECONNECTING, 
		  RPL_TRACEUNKNOWN, RPL_TRACEUSER, RPL_TRACECLASS, 
		  RPL_TRACEHANDSHAKE, RPL_TRACEOPERATOR,
		  RPL_TRACESERVER, RPL_TRACENEWTYPE, 0 },
		{ 0 },
		{ ERR_NOSUCHSERVER, 0 },
		handle_default
	},
	
 /* SUMMON <user> [<server>]*/
	{"SUMMON",
		{ 0 },
		{ RPL_SUMMONING, 0 },
		{ ERR_NORECIPIENT, ERR_FILEERROR, ERR_NOLOGIN, ERR_NOSUCHSERVER, 0 },
		handle_default
	},
	
 /* USERS [<server>]*/
	{"USERS",
		{ RPL_USERSSTART, RPL_USERS, RPL_NOUSERS, 0 },
		{ RPL_ENDOFUSERS, 0 },
		{ ERR_NOSUCHSERVER, ERR_FILEERROR, ERR_USERSDISABLED, 0 },
		handle_default
	},
	
 /* USERHOST <nickname>{ <nickname>}{ ...}*/
	{"USERHOST",
		{ 0 },
		{ RPL_USERHOST, 0 },
		{ ERR_NEEDMOREPARAMS, 0 },
		handle_default
	},
		
 /* ISON <nickname>{ <nickname>}{ ...} */
	{"ISON",
		{ 0 },
		{ RPL_ISON, 0 },
		{ ERR_NEEDMOREPARAMS, 0 },
	    handle_default
	},

/* JOIN <channel>{,<channel>} [<key>{,<key>}] */
	{"JOIN",
		{ RPL_TOPIC, RPL_TOPICWHOTIME, RPL_CREATIONTIME, 0 },
		{ 0 },
		{ ERR_NEEDMOREPARAMS, ERR_BANNEDFROMCHAN,
		  ERR_INVITEONLYCHAN, ERR_BADCHANNELKEY,
		  ERR_CHANNELISFULL, ERR_BADCHANMASK,
		  ERR_NOSUCHCHANNEL, ERR_TOOMANYCHANNELS, 0 },
		handle_default
	},

 /* PART <channel> *( "," <channel> ) [ <Part Message> ] */
	{ "PART",
		{ 0 },
		{ 0 },
		{ ERR_NEEDMOREPARAMS, ERR_NOSUCHCHANNEL, ERR_NOTONCHANNEL, 0 },
		handle_default
	},

 /* NICK <nickname> */
	{ "NICK",
		{ 0 },
		{ 0 },
		{ ERR_NONICKNAMEGIVEN, ERR_ERRONEUSNICKNAME, ERR_NICKNAMEINUSE,
	      ERR_UNAVAILRESOURCE, ERR_RESTRICTED, ERR_NICKCOLLISION, 
		  ERR_NICKTOOFAST, 0 },
		handle_default
	},

 /* USER <username> <hostname> <servername> <realname> */
	{ "USER",
		{ 0 },
		{ 0 },
		{ ERR_NEEDMOREPARAMS, ERR_ALREADYREGISTERED, 0 },
		handle_default
	},

 /* QUIT [<quit message>] */
	{ "QUIT", 
		{ 0 },
		{ 0 },
		{ 0 },
		handle_default
	},

 /* OPER <name> <password> */
	{ "OPER",
	    { 0 },
	    { RPL_YOUREOPER, 0 },
		{ ERR_NEEDMOREPARAMS, ERR_NOOPERHOST, ERR_PASSWDMISMATCH, 0 },
		handle_default
	},
	
 /* MODE <nick> <mode> */
	{ "MODE", 
		{  /* Replies to channel mode queries */
			RPL_BANLIST, RPL_EXCEPTLIST, RPL_INVITELIST, 
			
			0 },
		{ 
			/* Replies to user mode queries */
			RPL_UMODEIS, 

			/* Replies to channel mode queries */
			RPL_CHANNELMODEIS, RPL_ENDOFBANLIST, RPL_ENDOFEXCEPTLIST,
			RPL_ENDOFINVITELIST, RPL_UNIQOPIS,
			
			0 },
		{ 
			/* Common replies */
			ERR_NEEDMOREPARAMS, 

			/* Replies to user mode queries */
			ERR_UMODEUNKNOWNFLAG, ERR_USERSDONTMATCH, 
			
			/* Replies to channel mode queries */
			ERR_USERNOTINCHANNEL, ERR_KEYSET, ERR_CHANOPPRIVSNEEDED,
			ERR_UNKNOWNMODE, ERR_NOCHANMODES, 
			
			0 },
		handle_default
	},
	
 /* SERVICE <nick> <reserved> <distribution> <type> <reserved> <info> */
	{ "SERVICE", 
		{ 0 },
		{ RPL_YOURESERVICE, RPL_YOURHOST, RPL_MYINFO, 0 },
		{ ERR_ALREADYREGISTERED, ERR_NEEDMOREPARAMS, ERR_ERRONEUSNICKNAME, 0 },
		handle_default
	},
	
 /* SQUIT <server> <comment> */
	{ "SQUIT",
		{ 0 },
		{ 0 },
		{ ERR_NOPRIVILEGES, ERR_NEEDMOREPARAMS, ERR_NOSUCHSERVER, 0 },
		handle_default
	},
	
 /* INVITE <nick> <channel> */
	{ "INVITE", 
		{ 0 },
		{ RPL_INVITING, RPL_AWAY, 0 },
		{ ERR_NEEDMOREPARAMS, ERR_NOTONCHANNEL, ERR_NOSUCHNICK, 
			ERR_CHANOPPRIVSNEEDED, ERR_USERONCHANNEL, 0 },
		handle_default
	},
	
/* KICK <channel> * ( "," <channel> ) <user> *( "," <user> ) [<comment>] */
	{ "KICK",
		{ 0 },
		{ 0 },
		{ ERR_NEEDMOREPARAMS, ERR_BADCHANMASK, ERR_USERNOTINCHANNEL,
			ERR_NOSUCHCHANNEL, ERR_CHANOPPRIVSNEEDED, ERR_NOTONCHANNEL, 0 },
		handle_default
	},
 
 /* PRIVMSG <msgtarget> <text> */
	{ "PRIVMSG",
		{ 0 },
		{ RPL_AWAY, 0 },
		{ ERR_NORECIPIENT, ERR_NOTEXTTOSEND, ERR_CANNOTSENDTOCHAN, 
			ERR_NOTOPLEVEL, ERR_TOOMANYTARGETS, ERR_WILDTOPLEVEL, 
			ERR_NOSUCHNICK, ERR_NOSUCHCHANNEL, ERR_BLOCKING_NOTID, 0 },
		handle_default
	},
 
 /* MOTD [<target>] */
	{ "MOTD",
		{ RPL_MOTDSTART, RPL_MOTD, 0 },
		{ RPL_ENDOFMOTD, 0 },
		{ ERR_NOMOTD, 0 },
		handle_default
	},

 /* LUSERS [ <mask> [ <target> ] ] */
	{ "LUSERS",
		{ RPL_LUSERCLIENT, RPL_LUSEROP, RPL_LUSERUNKNOWN, RPL_LUSERCHANNELS, 
			RPL_LUSERME, 0 },
		{ 0 },
		{ ERR_NOSUCHSERVER, 0 },
		handle_default
	},
	
 /* CONNECT <target> <port> [ <remote server> ] */
	{ "CONNECT",
		{ 0 },
		{ 0 },
		{ ERR_NOSUCHSERVER, ERR_NEEDMOREPARAMS, ERR_NOPRIVILEGES, 0 },
		handle_default
	},

 /* ADMIN [ <target> ] */
	{ "ADMIN",
		{ RPL_ADMINME, RPL_ADMINLOC2, RPL_ADMINLOC1, 
			RPL_ADMINLOC3, 0 },
		{ 0 },
		{ ERR_NOSUCHSERVER, 0 },
		handle_default
	},
	
 /* INFO [ <target> ] */
	{ "INFO",
		{ RPL_INFO, 0 },
		{ RPL_ENDOFINFO, 0 },
		{ ERR_NOSUCHSERVER, 0 },
		handle_default
	},
	
 /* SERVLIST [ <mask> [ <type> ] ] */
	{ "SERVLIST",
		{ RPL_SERVLIST, 0},
		{ RPL_SERVLISTEND, 0 },
		{ 0 },
		handle_default
	},
	
 /* SQUERY <servicename> <text> */
	/* Same responses as for PRIVMSG */
	{ "SQUERY",
		{ 0 },
		{ RPL_AWAY, 0 },
		{ ERR_NORECIPIENT, ERR_NOTEXTTOSEND, ERR_CANNOTSENDTOCHAN, 
			ERR_NOTOPLEVEL, ERR_TOOMANYTARGETS, ERR_WILDTOPLEVEL, 
			ERR_NOSUCHNICK, 0 },
		handle_default
	},
	
 /* KILL <nick> <comment> */
	{ "KILL",
		{ 0 },
		{ 0 },
		{ ERR_NOPRIVILEGES, ERR_NEEDMOREPARAMS, ERR_NOSUCHNICK, ERR_CANTKILLSERVER, 0 },
		handle_default
	},

 /* AWAY [ <text> ] */
	{ "AWAY",
		{ 0 },
		{ RPL_UNAWAY, RPL_NOWAWAY, 0 },
		{ 0 },
		handle_default
	},

 /* REHASH */
	{ "REHASH",
		{ 0 },
		{ RPL_REHASHING, 0 },
		{ ERR_NOPRIVILEGES, 0 },
		handle_default
	},

 /* DIE */
	{ "DIE",
		{ 0 },
		{ 0 },
		{ ERR_NOPRIVILEGES, 0 },
		handle_default
	},

 /* RESTART */
	{ "RESTART",
		{ 0 },
		{ 0 },
		{ ERR_NOPRIVILEGES, 0 },
		handle_default
	},

 /* WALLOPS */
	{ "WALLOPS",
		{ 0 },
		{ 0 },
		{ ERR_NEEDMOREPARAMS, 0 },
		handle_default
	},

/* PONG */
	{ "PONG",
		{ 0 },
		{ 0 },
		{ ERR_NOORIGIN, ERR_NOSUCHSERVER, 0 },
		handle_default
	},

/* NOTICE */
	{ "NOTICE",
		{ 0 },
		{ 0 },
		{ 0 },
		handle_default
	},

	{ NULL }
};

static struct query unknown_query = { 
	NULL,
	{ 0 },
	{ 0 },
	{ ERR_UNKNOWNCOMMAND, 0 },
	handle_default
};

static void handle_465(struct network *n, struct line *l)
{
	log_network(LOG_ERROR, n, "Banned from server: %s", l->args[1]);
}

static void handle_451(struct network *n, struct line *l)
{
	log_network(LOG_ERROR, n, "Not registered error, this is probably a bug...");
}

static void handle_462(struct network *n, struct line *l)
{
	log_network(LOG_ERROR, n, "Double registration error, this is probably a bug...");
}

static void handle_463(struct network *n, struct line *l)
{
	log_network(LOG_ERROR, n, "Host not privileged to connect");
}

static void handle_464(struct network *n, struct line *l)
{
	log_network(LOG_ERROR, n, "Password mismatch");
}

/* List of responses that should be sent to all clients */
static int response_all[] = { RPL_NOWAWAY, RPL_UNAWAY, RPL_NAMREPLY, 
	RPL_ENDOFNAMES, ERR_NEEDREGGEDNICK, RPL_UMODEIS, 
	ERR_NO_OP_SPLIT, 0 };
static int response_none[] = { ERR_NOMOTD, RPL_ENDOFMOTD, 0 };
static struct {
	int response;
	void (*handler) (struct network *n, struct line *);
} response_handler[] = {
	{ ERR_PASSWDMISMATCH, handle_464 },
	{ ERR_ALREADYREGISTERED, handle_462 },
	{ ERR_NOPERMFORHOST, handle_463 },
	{ ERR_NOTREGISTERED, handle_451 },
	{ ERR_YOUREBANNEDCREEP, handle_465 },
	{ 0, NULL }
};

static int is_reply(const int *replies, int r)
{
	int i;
	g_assert(replies);

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
	struct query_stack *s, *p = NULL;
	const struct client *c = NULL;
	int n;
	int i;
	
	g_assert(network);
	g_assert(l->args[0]);

	n = atoi(l->args[0]);

	/* Find a request that this response is a reply to */
	for (s = stack; s; s = s->next) {
		if(s->network == network && 
		   (is_reply(s->query->replies, n) || 
			is_reply(s->query->errors, n) ||
			is_reply(s->query->end_replies, n))) {
			
			/* Send to client that queried, if that client still exists */
			if (s->client && verify_client(s->network, s->client)) {
				c = s->client;
				client_send_line(s->client, l);
			}

			if(!is_reply(s->query->replies, n)) {
				/* Remove from stack */
				if(!p)stack = s->next;	
				else p->next = s->next;
				g_free(s);
			}

			return;
		}
		p = s; 
	}

	/* See if this is a response that should be sent to all clients */
	for (i = 0; response_all[i]; i++) {
		if (response_all[i] == n) {
			clients_send(network->clients, l, c);
			return;
		}
	}

	/* See if this is a response that shouldn't be sent to clients at all */
	for (i = 0; response_none[i]; i++) {
		if (response_none[i] == n) {
			return;
		}
	}

	/* Handle response using custom function */
	for (i = 0; response_handler[i].handler; i++) {
		if (response_handler[i].response == n) {
			response_handler[i].handler(network, l);
			return;
		}
	}

	if (!c) {
		log_network(LOG_WARNING, network, "Unable to redirect response %s", l->args[0]);
		clients_send(network->clients, l, NULL);
	}
}

void redirect_clear(const struct network *net)
{
	struct query_stack *q, *p = NULL, *n;

	g_assert(net);

	q = stack;
	while (q) {
		if (q->network != net) {
			p = q;
			q = q->next;
			continue;
		}

		/* Remove from stack */
		if(!p)stack = q->next;	
		else p->next = q->next;
		n = q->next;
		g_free(q);
		q = n;
	}
}

void redirect_record(const struct network *n, struct client *c, const struct line *l)
{
	struct query *q;

	g_assert(n);
	g_assert(l);
	g_assert(l->args[0]);

	q = find_query(l->args[0]);
	if(!q) {
		if (c) {
			log_client(LOG_WARNING, c, "Unknown command from client: %s", l->args[0]);
		} else {
			log_network(LOG_WARNING, n, "Sending unknown command '%s'", l->args[0]);
		}

		q = &unknown_query;
	}

	/* Push it up the stack! */
	q->handle(l, n, c, q);
}

static int handle_default(const struct line *l, const struct network *n, struct client *c, struct query *q)
{
	struct query_stack *s = g_new(struct query_stack,1);
	g_assert(l);
	g_assert(n);
	g_assert(q);
	s->network = n;
	s->client = c;
	s->time = time(NULL);
	s->query = q;
	s->next = stack;
	stack = s;
	return 1;
}

static int handle_topic(const struct line *l, const struct network *n, struct client *c, struct query *q)
{
	if(l->args[2])return 0;
	return handle_default(l,n,c,q);
}
