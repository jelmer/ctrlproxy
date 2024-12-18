/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2007 Jelmer Vernooĳ <jelmer@jelmer.uk>

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

#ifndef __CTRLPROXY_ISUPPORT_H__
#define __CTRLPROXY_ISUPPORT_H__

#ifndef G_GNUC_NULL_TERMINATED
#define G_GNUC_NULL_TERMINATED
#endif

#ifndef G_GNUC_WARN_UNUSED_RESULT
#define G_GNUC_WARN_UNUSED_RESULT
#endif

/**
 * @file
 * @brief Network information
 */

enum log_level;

enum casemapping {
		CASEMAP_UNKNOWN = 0,
		CASEMAP_RFC1459,
		CASEMAP_ASCII,
		CASEMAP_STRICT_RFC1459
	};


enum chanmode_type {
	CHANMODE_UNKNOWN = 0,
	CHANMODE_NICKLIST,
	CHANMODE_SETTING,
	CHANMODE_OPT_SETTING,
	CHANMODE_BOOL
};

/**
 * Information about a network (doesn't change between connects or
 * servers).
 */
struct irc_network_info
{
	/** Network name. */
	char *name;

	char *server;

	/** List of characters indicating supported user modes. */
	char *supported_user_modes;

	/** List of characters indicating supported channel modes. */
	char *supported_channel_modes;

	/** Case mapping used for nick- and channel name comparing */
	enum casemapping casemapping;

	/** A list of channel modes a person can get and the respective
	 * prefix a channel or nickname will get in case the person has it */
	char *prefix;

	/** The supported channel prefixes */
	char *chantypes;

	/** Server supported character set */
	char *charset;

	/**
	 * This is a list of channel modes according to 4 types.
	 * A = Mode that adds or removes a nick or address to a list. Always has a parameter.
	 * B = Mode that changes a setting and always has a parameter.
	 * C = Mode that changes a setting and only has a parameter when set.
	 * D = Mode that changes a setting and never has a parameter.
	 */
	char **chanmodes;
	char *chanmodes_a;
	char *chanmodes_b;
	char *chanmodes_c;
	char *chanmodes_d;

	/** Maximum number of channels allowed to join by channel prefix. */
	/* FIXME: Should be a hash table */
	char *chanlimit;

	/** Maximum number entries in the list per mode. */
	/* FIXME: Should be a hash table */
	char *maxlist;

	/** The server supports extensions for the LIST command.
	 * The tokens specify which extension are supported. *
	 */
	gboolean elist_mask_search;
	gboolean elist_inverse_mask_search;
	gboolean elist_usercount_search;
	gboolean elist_creation_time_search;
	gboolean elist_topic_search;

	/** Specifies what extbans are supported by the server.
	 * The prefix defines which character indicates an extban and the types
	 * defines which extbans the server supports.  */

	char *extban_prefix;
	char *extban_supported;

	/** The ID length for channels with an ID. The prefix says for which
	 * channel type it is, and the number how long it is. See RFC 2811 for
	 * more information. */
	/* FIXME: Should be a hash table */
	char *idchan;

	/** The server support ban exceptions (e mode). See
	 * RFC 2811 for more information. */
	char excepts_mode;

	/** The server support invite exceptions (+I mode). See
	 * RFC 2811 for more information. */
	char invex_mode;

	/** The server supports dead mode (+d). */
	char deaf_mode;

	/** The server supports messaging channel member who have a
	 * certain status or higher. The status is one of the letters from
	 * PREFIX. */
	char *statusmsg;

	/** IRCD application used on the server */
	char *ircd;

	/** Maximum key length */
	int keylen;

	/** Server supports SSL */
	gboolean ssl;

	/** The server support the SILENCE command.  */
	gboolean silence;

	/** Server supports HybridIRC Connection Notices */
	gboolean hcn;

	/**
	 * The number is the maximum number of allowed entries in the silence
	 * list. */
	int silence_limit;

	/** Maximum channel name length */
	int channellen;

	/** The max length of an away message */
	int awaylen;

	/** Maximum kick comment length */
	int kicklen;

	/** Maximum targets allowed for PRIVMSG and NOTICE commands */
	int maxtargets;

	/** Maximum nickname length */
	int nicklen;

	/** Maximum username length */
	int userlen;

	/** Maximum hostname length */
	int hostlen;

	/** Maximum number of channels allowed to join */
	int maxchannels;

	/** Maximum topic length */
	int topiclen;

	/** Maximum number of bans per channel */
	int maxbans;

	/** Maximum number of channel modes with parameter allowed per MODE
	 * command  */
	int maxmodes;

	/** The server supports messaging channel operators (NOTICE @#channel) */
	gboolean wallchops;

	/** Notice to +#channel goes to all voiced persons */
	gboolean wallvoices;

	/** Server supports RFC 2812 features */
	gboolean rfc2812;

	/** Server gives extra penalty to some commands instead of the normal 2
	 * seconds per message and 1 second for every 120 bytes in a message */
	gboolean penalty;

	/** Forced nick changes: The server may change the nickname without the
	 * client sending a NICK message */
	gboolean forced_nick_changes;

	/** The LIST is sent in multiple iterations so send queue won't fill and
	 * kill the client connection. */
	gboolean safelist;

	/** The USERIP command exists */
	gboolean userip;

	/** The CPRIVMSG command exists, used for mass messaging people in specified
	 * channel (CPRIVMSG channel nick,nick2,... :text) */
	gboolean cprivmsg;

	/** The CNOTICE command exists, just like CPRIVMSG */
	gboolean cnotice;

	/** The KNOCK command exists */
	gboolean knock;

	/** Server supports virtual channels. See  vchans.txt for more information */
	gboolean vchannels;

	/** The WHO command uses WHOX protocol */
	gboolean whox;

	/** The server supports server side ignores via the +g user mode */
	gboolean callerid;

	/** [Deprecated] The same as CALLERID */
	gboolean accept;

	/** Support for CAPAB
	 * http://www3.ietf.org/proceedings/03mar/I-D/draft-baudis-irc-capab-00.txt
	 */
	gboolean capab;

	/** Maximum number of arguments per command. */
	int maxpara;

	/** The NAMESX extension is supported by the server. See
	 * http://www.inspircd.org/wiki/NAMESX_Module for details */
	gboolean namesx;

	/** This server uses SECURELIST, meaning that LIST can not be
	 * run 60 seconds within connect. See
	 * http://www.inspircd.org/wiki/Secure_LIST_Module for details. */
	gboolean securelist;

	/** Number of watches allowed */
	int watch;

	/** Server supports /fpart or /remove command */
	gboolean remove;

	/** Server supports /map command */
	gboolean map;

	/** Server supports operoverride */
	gboolean operoverride;

	/** Server supports vbanlist */
	gboolean vbanlist;

	/** User host names in who list */
	gboolean uhnames;

	/** Extended silence (?) */
	gboolean esilence;
};

G_GNUC_WARN_UNUSED_RESULT G_MODULE_EXPORT char *network_info_string(struct irc_network_info *info);
G_GNUC_WARN_UNUSED_RESULT G_MODULE_EXPORT gboolean is_channelname(const char *name, const struct irc_network_info *s);
G_GNUC_WARN_UNUSED_RESULT G_MODULE_EXPORT gboolean is_prefix(char p, const struct irc_network_info *n);
G_GNUC_WARN_UNUSED_RESULT G_MODULE_EXPORT char get_prefix_by_mode(char p, const struct irc_network_info *n);
G_MODULE_EXPORT int irccmp(const struct irc_network_info *n, const char *a, const char *b);
G_MODULE_EXPORT int ircncmp(const struct irc_network_info *n, const char *a, const char *b, size_t len);
G_GNUC_WARN_UNUSED_RESULT G_MODULE_EXPORT const char *get_charset(const struct irc_network_info *n);
G_MODULE_EXPORT void network_info_parse(struct irc_network_info *info, const char *parameter);
G_MODULE_EXPORT enum chanmode_type network_chanmode_type(char m, struct irc_network_info *n);
G_GNUC_WARN_UNUSED_RESULT G_MODULE_EXPORT struct irc_network_info *network_info_init(void);
G_MODULE_EXPORT void free_network_info(struct irc_network_info *info);
G_MODULE_EXPORT void network_info_log(enum log_level l,
				const struct irc_network_info *info, const char *fmt, ...);

#endif /* __CTRLPROXY_ISUPPORT_H__ */
