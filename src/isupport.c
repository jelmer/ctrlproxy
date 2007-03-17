/*
	ctrlproxy: A modular IRC proxy
	(c) 2005 Jelmer Vernooij <jelmer@nl.linux.org>

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

#include "internals.h"

#define DEFAULT_PREFIX		"(ov)@+"
#define DEFAULT_CHANTYPES 	"#&"
#define DEFAULT_CHARSET		"iso8859-15"

char *network_info_string(struct network_info *info)
{
	char *ret = NULL;
	GList *fs = NULL, *gl;
	char *casemap = NULL;

	if (info->name != NULL)
		fs = g_list_append(fs, g_strdup_printf("NETWORK=%s", info->name));

	switch (info->casemapping) {
	default:
	case CASEMAP_RFC1459:
		casemap = g_strdup("CASEMAPPING=rfc1459");
		break;
	case CASEMAP_STRICT_RFC1459:
		casemap = g_strdup("CASEMAPPING=strict-rfc1459");
		break;
	case CASEMAP_ASCII:
		casemap = g_strdup("CASEMAPPING=ascii");
		break;
	}

	if (casemap != NULL)
		fs = g_list_append(fs, casemap);

	if (info->forced_nick_changes)
		fs = g_list_append(fs, g_strdup("FNC"));

	if (info->charset != NULL)
		fs = g_list_append(fs, g_strdup_printf("CHARSET=%s", info->charset));

	if (info->nicklen != 0)
		fs = g_list_append(fs, g_strdup_printf("NICKLEN=%d", info->nicklen));

	if (info->userlen != 0)
		fs = g_list_append(fs, g_strdup_printf("USERLEN=%d", info->userlen));

	if (info->hostlen != 0)
		fs = g_list_append(fs, g_strdup_printf("HOSTLEN=%d", info->hostlen));

	if (info->channellen != 0)
		fs = g_list_append(fs, g_strdup_printf("CHANNELLEN=%d", info->channellen));

	if (info->awaylen != 0)
		fs = g_list_append(fs, g_strdup_printf("AWAYLEN=%d", info->awaylen));

	if (info->kicklen != 0)
		fs = g_list_append(fs, g_strdup_printf("KICKLEN=%d", info->kicklen));

	if (info->topiclen != 0)
		fs = g_list_append(fs, g_strdup_printf("TOPICLEN=%d", info->topiclen));

	if (info->maxchannels != 0)
		fs = g_list_append(fs, g_strdup_printf("MAXCHANNELS=%d", info->maxchannels));

	if (info->maxtargets != 0)
		fs = g_list_append(fs, g_strdup_printf("MAXTARGETS=%d", info->maxtargets));

	if (info->maxbans != 0)
		fs = g_list_append(fs, g_strdup_printf("MAXBANS=%d", info->maxbans));

	if (info->maxmodes != 0)
		fs = g_list_append(fs, g_strdup_printf("MODES=%d", info->maxmodes));

	if (info->wallchops)
		fs = g_list_append(fs, g_strdup("WALLCHOPS"));

	if (info->wallvoices)
		fs = g_list_append(fs, g_strdup("WALLVOICES"));

	if (info->rfc2812)
		fs = g_list_append(fs, g_strdup("RFC2812"));

	if (info->penalty)
		fs = g_list_append(fs, g_strdup("PENALTY"));

	if (info->safelist)
		fs = g_list_append(fs, g_strdup("SAFELIST"));
	
	if (info->userip)
		fs = g_list_append(fs, g_strdup("USERIP"));

	if (info->cprivmsg)
		fs = g_list_append(fs, g_strdup("CPRIVMSG"));

	if (info->cnotice)
		fs = g_list_append(fs, g_strdup("CNOTICE"));

	if (info->knock)
		fs = g_list_append(fs, g_strdup("KNOCK"));

	if (info->vchannels)
		fs = g_list_append(fs, g_strdup("VCHANNELS"));

	if (info->whox)
		fs = g_list_append(fs, g_strdup("WHOX"));

	if (info->callerid)
		fs = g_list_append(fs, g_strdup("CALLERID"));

	if (info->accept)
		fs = g_list_append(fs, g_strdup("ACCEPT"));

	if (info->keylen != 0)
		fs = g_list_append(fs, g_strdup_printf("KEYLEN=%d", info->keylen));

	if (info->silence != 0)
		fs = g_list_append(fs, g_strdup_printf("SILENCE=%d", info->silence));

	if (info->chantypes != NULL)
		fs = g_list_append(fs, g_strdup_printf("CHANTYPES=%s", info->chantypes));

	if (info->chanmodes != NULL) {
		char *tmp = g_strjoinv(",", info->chanmodes);
		fs = g_list_append(fs, g_strdup_printf("CHANMODES=%s", tmp));
		g_free(tmp);
	}

	if (info->prefix != NULL) 
		fs = g_list_append(fs, g_strdup_printf("PREFIX=%s", info->prefix));

	ret = list_make_string(fs);

	for (gl = fs; gl; gl = gl->next)
		g_free(gl->data);

	g_list_free(fs);

	return ret;
}

void network_info_parse(struct network_info *info, const char *parameter)
{
	char *sep;
	char *key, *val;

	sep = strchr(parameter, '=');

	if (!sep) { 
		key = g_strdup(parameter);
		val = NULL; 
	} else {
		key = g_strndup(parameter, sep - parameter);
		val = g_strdup(sep+1);
	}
	
	if(!g_strcasecmp(key, "CASEMAPPING")) {
		if(!g_strcasecmp(val, "rfc1459")) {
			info->casemapping = CASEMAP_RFC1459;
		} else if(!g_strcasecmp(val, "strict-rfc1459")) {
			info->casemapping = CASEMAP_STRICT_RFC1459;
		} else if(!g_strcasecmp(val, "ascii")) {
			info->casemapping = CASEMAP_ASCII;
		} else {
			info->casemapping = CASEMAP_UNKNOWN;
			log_global(LOG_WARNING, "Unknown CASEMAPPING value '%s'", val);
		}
	} else if(!g_strcasecmp(key, "NETWORK")) {
		g_free(info->name);
		info->name = g_strdup(val);
	} else if(!g_strcasecmp(key, "NICKLEN")) {
		info->nicklen = atoi(val);
	} else if(!g_strcasecmp(key, "USERLEN")) {
		info->userlen = atoi(val);
	} else if(!g_strcasecmp(key, "HOSTLEN")) {
		info->hostlen = atoi(val);
	} else if(!g_strcasecmp(key, "CHANNELLEN")) {
		info->channellen = atoi(val);
	} else if(!g_strcasecmp(key, "AWAYLEN")) {
		info->awaylen = atoi(val);
	} else if(!g_strcasecmp(key, "KICKLEN")) {
		info->kicklen = atoi(val);
	} else if(!g_strcasecmp(key, "TOPICLEN")) {
		info->topiclen = atoi(val);
	} else if(!g_strcasecmp(key, "MAXCHANNELS")) {
		info->maxchannels = atoi(val);
	} else if(!g_strcasecmp(key, "MAXTARGETS")) {
		info->maxtargets = atoi(val);
	} else if(!g_strcasecmp(key, "MAXBANS")) {
		info->maxbans = atoi(val);
	} else if(!g_strcasecmp(key, "MODES")) {
		info->maxmodes = atoi(val);
	} else if(!g_strcasecmp(key, "WALLCHOPS")) {
		info->wallchops = TRUE;
	} else if(!g_strcasecmp(key, "WALLVOICES")) {
		info->wallvoices = TRUE;
	} else if(!g_strcasecmp(key, "RFC2812")) {
		info->rfc2812 = TRUE;
	} else if(!g_strcasecmp(key, "PENALTY")) {
		info->penalty = TRUE;
	} else if(!g_strcasecmp(key, "FNC")) {
		info->forced_nick_changes = TRUE;
	} else if(!g_strcasecmp(key, "SAFELIST")) {
		info->safelist = TRUE;
	} else if(!g_strcasecmp(key, "USERIP")) {
		info->userip = TRUE;
	} else if(!g_strcasecmp(key, "CPRIVMSG")) {
		info->cprivmsg = TRUE;
	} else if(!g_strcasecmp(key, "CNOTICE")) {
		info->cnotice = TRUE;
	} else if(!g_strcasecmp(key, "KNOCK")) {
		info->knock = TRUE;
	} else if(!g_strcasecmp(key, "VCHANNELS")) {
		info->vchannels = TRUE;
	} else if(!g_strcasecmp(key, "WHOX")) {
		info->whox = TRUE;
	} else if(!g_strcasecmp(key, "CALLERID")) {
		info->callerid = TRUE;
	} else if(!g_strcasecmp(key, "ACCEPT")) {
		info->accept = TRUE;
	} else if(!g_strcasecmp(key, "KEYLEN")) {
		info->keylen = atoi(val);
	} else if(!g_strcasecmp(key, "SILENCE")) {
		info->silence = atoi(val);
	} else if(!g_strcasecmp(key, "CHANTYPES")) {
		g_free(info->chantypes);
		info->chantypes = g_strdup(val);
	} else if(!g_strcasecmp(key, "CHANMODES")) {
		g_strfreev(info->chanmodes);
		info->chanmodes = g_strsplit(val, ",", 4);
		/* FIXME: Make sure info->chanmodes length is exactly 4 */
	} else if(!g_strcasecmp(key, "PREFIX")) {
		g_free(info->prefix);
		info->prefix = g_strdup(val);
	} else if(!g_strcasecmp(key, "CHARSET")) {
		g_free(info->charset);
		info->charset = g_strdup(val);
	} else {
		log_global(LOG_WARNING, "Unknown 005 parameter `%s'", key);
	}
	g_free(key);
	g_free(val);
}

void handle_005(struct network_state *s, struct line *l)
{
	int i;
	g_assert(s);
	g_assert(l);

	g_assert(l->argc >= 1);

	for (i = 3; i < l->argc-1; i++) 
		network_info_parse(&s->info, l->args[i]);
}

int irccmp(const struct network_info *n, const char *a, const char *b)
{
	switch(n?n->casemapping:CASEMAP_UNKNOWN) {
	default:
	case CASEMAP_UNKNOWN:
	case CASEMAP_RFC1459:
		return str_rfc1459cmp(a,b);
	case CASEMAP_ASCII:
		return str_asciicmp(a,b);
	case CASEMAP_STRICT_RFC1459:
		return str_strictrfc1459cmp(a,b);
	}

	return 0;
}

gboolean is_channelname(const char *name, const struct network_info *n)
{
	const char *chantypes = NULL;

	g_assert(name);

	if (n != NULL) {
		chantypes = n->chantypes;
	}

	if(chantypes == NULL) 
		chantypes = DEFAULT_CHANTYPES;
	
	if(strchr(chantypes, name[0])) 
		return TRUE;

	return FALSE;
}

gboolean is_prefix(char p, const struct network_info *n)
{
	const char *prefix = NULL;
	const char *pref_end;
	
	if (n != NULL) {
		prefix = n->prefix;
	}
	
	if (prefix == NULL) 
		prefix = DEFAULT_PREFIX;

	pref_end = strchr(prefix, ')');
	if (!pref_end)pref_end = prefix; else pref_end++;

	if(strchr(pref_end, p)) return TRUE;
	return FALSE;
}

const char *get_charset(const struct network_info *n)
{
	if (n != NULL && n->charset != NULL)
		return n->charset;

	return DEFAULT_CHARSET;
}

char get_prefix_by_mode(char mode, const struct network_info *n)
{
	const char *prefix = NULL;
	int i;
	char *pref_end;

	if (n != NULL) {
		prefix = n->prefix;
	}

	if (prefix == NULL) 
		prefix = DEFAULT_PREFIX;
	
	pref_end = strchr(prefix, ')');
	if(prefix[0] != '(' || !pref_end) {
		log_global(LOG_WARNING, "Malformed PREFIX data `%s'", prefix);
		return ' ';
	}
	pref_end++;
	prefix++;

	for(i = 0; pref_end[i]; i++) {
		if(prefix[i] == mode) return pref_end[i];
	}
	return ' ';
}

const static char *default_chanmodes[] = { 
	"beI", "k" , "l" , "imnpsta" 
};

int network_chanmode_type(char m, struct network_info *n)
{
	int i;
	for (i = 0; i < 4; i++) {
		if (strchr((n && n->chanmodes)?n->chanmodes[i]:default_chanmodes[i], m)) {
			return i;
		}
	}

	return 3;
}
