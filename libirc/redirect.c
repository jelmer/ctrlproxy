/*
	ctrlproxy: A modular IRC proxy
	Send numerics to the right places
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

#include "internals.h"
#include <string.h>
#include "irc.h"

/* TODO: Clean up stack occasionally */

struct query_stack *new_query_stack(void (*ref_userdata) (void *), void (*unref_userdata) (void *))
{
	struct query_stack *stack = g_new0(struct query_stack, 1);
	if (stack == NULL) {
		return NULL;
	}
	stack->ref_userdata = ref_userdata;
	stack->unref_userdata = unref_userdata;
	return stack;
}

static int handle_default(const struct irc_line *, struct query_stack *stack,
						  void *userdata, struct query *);
static int handle_topic(const struct irc_line *, struct query_stack *stack,
						void *userdata, struct query *);

static struct query queries[] = {
/* Commands that get a one-client reply:
 * WHOIS [<server>] <nickmask>[,<nickmask>[,...]] */
	{"WHOIS",
		{ RPL_WHOISUSER, RPL_WHOISCHANNELS, RPL_AWAY,
		  RPL_WHOISIDLE, RPL_WHOISCHANNELS, RPL_WHOISIP,
		  RPL_WHOISSERVER, RPL_WHOISOPERATOR, RPL_WHOISACTUALLY,
		  RPL_WHOISSSL, RPL_WHOISACCOUNT, RPL_WHOISIDENTIFIED,
		  RPL_WHOISSERVEROPERATOR, RPL_WHOISNETWORKSERVICE,
		  RPL_WHOISOPERPRIVS, ERR_NOSUCHNICK, 0 },
		{ RPL_ENDOFWHOIS, RPL_TRYAGAIN, 0 },
		{ ERR_NOSUCHSERVER, ERR_NONICKNAMEGIVEN, 0 },
		handle_default
	},

	/* WHO [<name> [<o>]] */
	{"WHO",
	    { RPL_WHOREPLY, RPL_WHOSPCRPL, 0 },
		{ RPL_ENDOFWHO, RPL_TRYAGAIN, 0 },
		{ ERR_NOSUCHSERVER, 0 },
		handle_default
	},

	/* NAMES [<channel>{,<channel>}]*/
	{"NAMES",
		{ RPL_NAMREPLY, 0 },
		{ RPL_ENDOFNAMES, RPL_TRYAGAIN, 0 },
		{ ERR_TOOMANYMATCHES, ERR_NOSUCHSERVER, 0 },
		handle_default
	},

 /* LIST [<channel>{,<channel>} [<server>]*/
	{"LIST",
		{ RPL_LIST, RPL_LISTSTART, 0 },
		{ RPL_LISTEND, RPL_TRYAGAIN, 0 },
		{ ERR_TOOMANYMATCHES, ERR_NOSUCHSERVER, 0 },
		handle_default
	},
	
 /* TOPIC <channel> [<topic>]*/
	{"TOPIC",
		{ 0 },
		{ RPL_NOTOPIC, RPL_TOPIC, RPL_TRYAGAIN, 0 },
		{ ERR_NOTONCHANNEL, ERR_NEEDMOREPARAMS, ERR_CHANOPPRIVSNEEDED,
			ERR_NOCHANMODES, 0 },
		handle_topic
	},
	
 /* WHOWAS <nickname> [<count> [<server>]]*/
	{"WHOWAS",
		{ RPL_WHOWASUSER, RPL_WHOISSERVER, RPL_WHOWAS_TIME, ERR_WASNOSUCHNICK, 0 },
		{ RPL_ENDOFWHOWAS, RPL_TRYAGAIN, 0 },
		{ ERR_NONICKNAMEGIVEN, 0 },
		handle_default
	},
	
 /* STATS [<query> [<server>]]*/
	{"STATS",
		{ RPL_STATSCLINE, RPL_STATSILINE, RPL_STATSQLINE, RPL_STATSPLINE,
		  RPL_STATSLINKINFO, RPL_STATSCOMMANDS, RPL_STATSHLINE, RPL_STATSNLINE,
		  RPL_STATSKLINE, RPL_STATSLLINE, RPL_STATSUPTIME, RPL_STATSOLINE,
		  RPL_STATSTLINE,
		  0 },
		{ RPL_TRYAGAIN, RPL_ENDOFSTATS, 0 },
		{ ERR_NOSUCHSERVER, 0 },
		handle_default
	},
	
 /* VERSION [<server>]*/
	{"VERSION",
		{ 0 },
		{ RPL_VERSION, RPL_TRYAGAIN, 0 },
		{ ERR_NOSUCHSERVER, 0 },
		handle_default
	},
		
 /* LINKS [[<remote server>] <server mask>]*/
	{"LINKS",
		{ RPL_LINKS, 0 },
		{ RPL_ENDOFLINKS, RPL_TRYAGAIN, 0 },
		{ ERR_NOSUCHSERVER, 0 },
		handle_default
	},
	
 /* TIME [<server>]*/
	{"TIME",
		{ 0 },
		{ RPL_TIME, RPL_TRYAGAIN, 0 },
		{ ERR_NOSUCHSERVER, 0 },
		handle_default
	},

 /* TRACE [<server>]*/
	{"TRACE",
		{ RPL_TRACELINK, RPL_TRACECONNECTING,
		  RPL_TRACEUNKNOWN, RPL_TRACEUSER, RPL_TRACECLASS,
		  RPL_TRACEHANDSHAKE, RPL_TRACEOPERATOR,
		  RPL_TRACESERVER, RPL_TRACENEWTYPE, 0 },
		{ RPL_TRYAGAIN, 0 },
		{ ERR_NOSUCHSERVER, 0 },
		handle_default
	},
	
 /* SUMMON <user> [<server>]*/
	{"SUMMON",
		{ 0 },
		{ RPL_SUMMONING, RPL_TRYAGAIN, 0 },
		{ ERR_NORECIPIENT, ERR_FILEERROR, ERR_NOLOGIN, ERR_NOSUCHSERVER, 0 },
		handle_default
	},
	
 /* USERS [<server>]*/
	{"USERS",
		{ RPL_USERSSTART, RPL_USERS, RPL_NOUSERS, 0 },
		{ RPL_ENDOFUSERS, RPL_TRYAGAIN, 0 },
		{ ERR_NOSUCHSERVER, ERR_FILEERROR, ERR_USERSDISABLED, 0 },
		handle_default
	},

 /* USERHOST <nickname>{ <nickname>}{ ...}*/
	{"USERHOST",
		{ 0 },
		{ RPL_USERHOST, RPL_TRYAGAIN, 0 },
		{ ERR_NEEDMOREPARAMS, 0 },
		handle_default
	},
		
 /* ISON <nickname>{ <nickname>}{ ...} */
	{"ISON",
		{ 0 },
		{ RPL_ISON, RPL_TRYAGAIN, 0 },
		{ ERR_NEEDMOREPARAMS, 0 },
	    handle_default
	},

/* JOIN <channel>{,<channel>} [<key>{,<key>}] */
	{"JOIN",
		{ 0 },
		{ RPL_TRYAGAIN, 0 },
		{ ERR_NEEDMOREPARAMS, ERR_BANNEDFROMCHAN,
		  ERR_INVITEONLYCHAN, ERR_BADCHANNELKEY,
		  ERR_CHANNELISFULL, ERR_BADCHANMASK,
		  ERR_FORWARDING, ERR_NOSUCHCHANNEL,
		  ERR_ILLEGALCHANNELNAME, ERR_TOOMANYCHANNELS, 0 },
		handle_default
	},

 /* PART <channel> *( "," <channel> ) [ <Part Message> ] */
	{ "PART",
		{ 0 },
		{ RPL_TRYAGAIN, 0 },
		{ ERR_NEEDMOREPARAMS, ERR_NOSUCHCHANNEL, ERR_NOTONCHANNEL, 0 },
		handle_default
	},

 /* NICK <nickname> */
	{ "NICK",
		{ 0 },
		{ RPL_TRYAGAIN, 0 },
		{ ERR_NONICKNAMEGIVEN, ERR_ERRONEUSNICKNAME, ERR_NICKNAMEINUSE,
	      ERR_UNAVAILRESOURCE, ERR_RESTRICTED, ERR_NICKCOLLISION,
		  ERR_NICKTOOFAST, 0 },
		handle_default
	},

 /* PASS <password> <version> <flags> [<options>] */
	{ "PASS",
		{ 0 },
		{ 0 },
		{ ERR_NEEDMOREPARAMS, ERR_ALREADYREGISTERED, 0 },
		handle_default
	},

 /* USER <username> <hostname> <servername> <realname> */
	{ "USER",
		{ 0 },
		{ RPL_TRYAGAIN, 0 },
		{ ERR_NEEDMOREPARAMS, ERR_ALREADYREGISTERED, 0 },
		handle_default
	},

 /* QUIT [<quit message>] */
	{ "QUIT",
		{ 0 },
		{ RPL_TRYAGAIN, 0 },
		{ 0 },
		handle_default
	},

 /* OPER <name> <password> */
	{ "OPER",
	    { 0 },
	    { RPL_YOUREOPER, RPL_TRYAGAIN, 0 },
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
			
			RPL_TRYAGAIN,
			0 },
		{
			/* Common replies */
			ERR_NEEDMOREPARAMS,

			/* Replies to user mode queries */
			ERR_UMODEUNKNOWNFLAG, ERR_USERSDONTMATCH,
			
			/* Replies to channel mode queries */
			ERR_USERNOTINCHANNEL, ERR_KEYSET, ERR_CHANOPPRIVSNEEDED,
			ERR_UNKNOWNMODE, ERR_NOCHANMODES, ERR_NOSUCHCHANNEL,
			ERR_ILLEGALCHANNELNAME,
			
			0 },
		handle_default
	},
	
 /* SERVICE <nick> <reserved> <distribution> <type> <reserved> <info> */
	{ "SERVICE",
		{ 0 },
		{ RPL_YOURESERVICE, RPL_YOURHOST, RPL_MYINFO, RPL_TRYAGAIN, 0 },
		{ ERR_ALREADYREGISTERED, ERR_NEEDMOREPARAMS, ERR_ERRONEUSNICKNAME, 0 },
		handle_default
	},
	
 /* SQUIT <server> <comment> */
	{ "SQUIT",
		{ 0 },
		{ RPL_TRYAGAIN, 0 },
		{ ERR_NOPRIVILEGES, ERR_NEEDMOREPARAMS, ERR_NOSUCHSERVER, 0 },
		handle_default
	},
	
 /* INVITE <nick> <channel> */
	{ "INVITE",
		{ 0 },
		{ RPL_INVITING, RPL_AWAY, RPL_TRYAGAIN, 0 },
		{ ERR_NEEDMOREPARAMS, ERR_NOTONCHANNEL, ERR_NOSUCHNICK,
			ERR_CHANOPPRIVSNEEDED, ERR_USERONCHANNEL, 0 },
		handle_default
	},
	
/* KICK <channel> * ( "," <channel> ) <user> *( "," <user> ) [<comment>] */
	{ "KICK",
		{ 0 },
		{ RPL_TRYAGAIN, 0 },
		{ ERR_NEEDMOREPARAMS, ERR_BADCHANMASK, ERR_USERNOTINCHANNEL,
			ERR_NOSUCHCHANNEL, ERR_CHANOPPRIVSNEEDED, ERR_NOTONCHANNEL, 0 },
		handle_default
	},

 /* PRIVMSG <msgtarget> <text> */
	{ "PRIVMSG",
		{ 0 },
		{ RPL_AWAY, RPL_TRYAGAIN, 0 },
		{ ERR_NORECIPIENT, ERR_NOTEXTTOSEND, ERR_CANNOTSENDTOCHAN,
			ERR_NOTOPLEVEL, ERR_TOOMANYTARGETS, ERR_WILDTOPLEVEL,
			ERR_NOSUCHNICK, ERR_NOSUCHCHANNEL, ERR_BLOCKING_NOTID, 0 },
		handle_default
	},

	{ "NICKSERV",
		{ 0 },
		{ RPL_TRYAGAIN, 0 },
		{ ERR_NOTEXTTOSEND, 0 },
		handle_default
	},

	{ "CHANSERV",
		{ 0 },
		{ RPL_TRYAGAIN, 0 },
		{ ERR_NOTEXTTOSEND, 0 },
		handle_default
	},

 /* MOTD [<target>] */
	{ "MOTD",
		{ RPL_MOTDSTART, RPL_MOTD, 0 },
		{ RPL_ENDOFMOTD, RPL_TRYAGAIN, 0 },
		{ ERR_NOMOTD, 0 },
		handle_default
	},

 /* LUSERS [ <mask> [ <target> ] ] */
	{ "LUSERS",
		{ RPL_LUSERCLIENT, RPL_LUSEROP, RPL_LUSERUNKNOWN, RPL_LUSERCHANNELS,
			RPL_LUSERME, RPL_STATSTLINE, 0 },
		{ RPL_TRYAGAIN, 0 },
		{ ERR_NOSUCHSERVER, 0 },
		handle_default
	},
	
 /* CONNECT <target> <port> [ <remote server> ] */
	{ "CONNECT",
		{ 0 },
		{ RPL_TRYAGAIN, 0 },
		{ ERR_NOSUCHSERVER, ERR_NEEDMOREPARAMS, ERR_NOPRIVILEGES, 0 },
		handle_default
	},

 /* ADMIN [ <target> ] */
	{ "ADMIN",
		{ RPL_ADMINME, RPL_ADMINLOC2, RPL_ADMINLOC1,
			RPL_ADMINLOC3, 0 },
		{ RPL_TRYAGAIN, 0 },
		{ ERR_NOSUCHSERVER, 0 },
		handle_default
	},
	
 /* INFO [ <target> ] */
	{ "INFO",
		{ RPL_INFO, 0 },
		{ RPL_TRYAGAIN, RPL_ENDOFINFO, 0 },
		{ ERR_NOSUCHSERVER, 0 },
		handle_default
	},
	
 /* SERVLIST [ <mask> [ <type> ] ] */
	{ "SERVLIST",
		{ RPL_SERVLIST, 0},
		{ RPL_TRYAGAIN, RPL_SERVLISTEND, 0 },
		{ 0 },
		handle_default
	},
	
 /* SQUERY <servicename> <text> */
	/* Same responses as for PRIVMSG */
	{ "SQUERY",
		{ 0 },
		{ RPL_AWAY, RPL_TRYAGAIN, 0 },
		{ ERR_NORECIPIENT, ERR_NOTEXTTOSEND, ERR_CANNOTSENDTOCHAN,
			ERR_NOTOPLEVEL, ERR_TOOMANYTARGETS, ERR_WILDTOPLEVEL,
			ERR_NOSUCHNICK, 0 },
		handle_default
	},
	
 /* KILL <nick> <comment> */
	{ "KILL",
		{ 0 },
		{ RPL_TRYAGAIN, 0 },
		{ ERR_NOPRIVILEGES, ERR_NEEDMOREPARAMS, ERR_NOSUCHNICK, ERR_CANTKILLSERVER, 0 },
		handle_default
	},

 /* AWAY [ <text> ] */
	{ "AWAY",
		{ 0 },
		{ RPL_UNAWAY, RPL_NOWAWAY, RPL_TRYAGAIN, 0 },
		{ 0 },
		handle_default
	},

 /* REHASH */
	{ "REHASH",
		{ 0 },
		{ RPL_REHASHING, RPL_TRYAGAIN, 0 },
		{ ERR_NOPRIVILEGES, 0 },
		handle_default
	},

 /* DIE */
	{ "DIE",
		{ 0 },
		{ RPL_TRYAGAIN, 0 },
		{ ERR_NOPRIVILEGES, 0 },
		handle_default
	},

 /* RESTART */
	{ "RESTART",
		{ 0 },
		{ RPL_TRYAGAIN, 0 },
		{ ERR_NOPRIVILEGES, 0 },
		handle_default
	},

/* PING */
	{ "PING",
		{ 0 },
		{ RPL_TRYAGAIN, 0 },
		{ ERR_NOORIGIN, 0 },
		handle_default
	},

 /* WALLOPS */
	{ "WALLOPS",
		{ 0 },
		{ RPL_TRYAGAIN, 0 },
		{ ERR_NEEDMOREPARAMS, 0 },
		handle_default
	},

/* PONG */
	{ "PONG",
		{ 0 },
		{ RPL_TRYAGAIN, 0 },
		{ ERR_NOORIGIN, ERR_NOSUCHSERVER, 0 },
		handle_default
	},

/* NOTICE */
	{ "NOTICE",
		{ 0 },
		{ RPL_TRYAGAIN, 0 },
		{ 0 },
		handle_default
	},

/* AUTH */
	{ "AUTH",
		{ 0 },
		{ RPL_TRYAGAIN, 0 },
		{ ERR_NEEDMOREPARAMS, ERR_ALREADYAUTHENTICATED, ERR_ALREADYREGISTERED,
		  ERR_AUTHENTICATIONFAILED, ERR_AUTHENTICATIONSUSPENDED,
		  ERR_BADCOMMAND, ERR_UNKNOWNPACKAGE, 0 },
		handle_default
	},

	{ "CAPAB",
		{ 0 },
		{ RPL_CAPAB, 0 },
		{ ERR_NEEDMOREPARAMS, 0 },
		handle_default
	},

	{ NULL }
};

static struct query unknown_query = {
	NULL,
	{ 0 },
	{ RPL_TRYAGAIN, 0 },
	{ ERR_UNKNOWNCOMMAND, 0 },
	handle_default
};

/**
 * Check whether reply r is part of a specified list
 *
 * @param replies List of possible replies.
 * @param r Response code.
 */
static int is_reply(const int *replies, int r)
{
	int i;
	g_assert(replies != NULL);

	for(i = 0; i < 20 && replies[i]; i++) {
		if (replies[i] == r) return 1;
	}
	return 0;
}

static struct query *find_query(char *name)
{
	int i;
	for(i = 0; queries[i].name; i++) {
		if (!base_strcmp(queries[i].name, name)) {
			return &queries[i];
		}
	}

	return NULL;
}

static void query_stack_free_entry(struct query_stack *stack, struct query_stack_entry *s)
{
	stack->unref_userdata(s->userdata);
	g_free(s);
}

void *query_stack_match_response(struct query_stack *stack, const struct irc_line *l)
{
	int n;
	void *ret = NULL;
	GList *gl;

	g_assert(l->args[0]);

	n = irc_line_respcode(l);

	/* Find a request that this response is a reply to */
	for (gl = stack->entries; gl; gl = gl->next) {
		struct query_stack_entry *s = gl->data;
		if (is_reply(s->query->replies, n) ||
			is_reply(s->query->errors, n) ||
			is_reply(s->query->end_replies, n)) {
			
			ret = s->userdata;

			/* Not a valid in-between reply ? Remove from stack */
			if (!is_reply(s->query->replies, n)) {
				/* Remove from stack */
				stack->entries = g_list_remove(stack->entries, s);
				query_stack_free_entry(stack, s);
			}

			return ret;
		}
	}

	return NULL;
}

void query_stack_clear(struct query_stack *stack)
{
	while (stack->entries != NULL) {
		struct query_stack_entry *e = stack->entries->data;
		stack->entries = g_list_remove(stack->entries, e);
		query_stack_free_entry(stack, e);
	}
}

void query_stack_free(struct query_stack *stack)
{
	query_stack_clear(stack);
	g_free(stack);
}

gboolean query_stack_record(struct query_stack *stack, void *userdata, const struct irc_line *l)
{
	struct query *q;
	gboolean ret = TRUE;

	g_assert(l);
	g_assert(l->args[0]);

	q = find_query(l->args[0]);
	if (q == NULL) {
		ret = FALSE;
		q = &unknown_query;
	}

	/* Push it up the stack! */
	q->handle(l, stack, userdata, q);

	return ret;
}

static int handle_default(const struct irc_line *l, struct query_stack *stack,
						  void *userdata, struct query *q)
{
	struct query_stack_entry *s = g_new(struct query_stack_entry, 1);
	g_assert(l != NULL);
	g_assert(q != NULL);
	stack->ref_userdata(userdata);
	s->userdata = userdata;
	s->time = time(NULL);
	s->query = q;
	stack->entries = g_list_append(stack->entries, s);
	return 1;
}

static int handle_topic(const struct irc_line *l, struct query_stack *stack, void *userdata, struct query *q)
{
	if (l->args[2] != NULL)
		return 0;
	return handle_default(l,stack,userdata,q);
}
