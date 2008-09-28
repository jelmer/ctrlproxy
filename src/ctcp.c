/* 
	ctrlproxy: A modular IRC proxy
	(c) 2002-2003 Jelmer Vernooij <jelmer@nl.linux.org>

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
#ifndef _WIN32
#include <sys/utsname.h>
#endif

/**
 * Represents a CTCP request
 */
struct ctcp_handle 
{
	struct irc_network *network;
	char *nick;
};

static char *toarg(va_list ap)
{
	char *arg;
	GList *list = NULL, *gl;
	int len = 4;
	char *ret, *pos;

	while((arg = va_arg(ap, char *))) {
		list = g_list_append(list, arg);
		len+=strlen(arg) + 2;
	}

	g_assert(list);

	ret = g_new0(char, len);
	ret[0] = '\001';
	pos = ret+1;
	strcpy(pos, list->data); pos += strlen(list->data);
	for (gl = list->next; gl; gl = gl->next) {
		*pos = ' '; 
		strcpy(pos+1, gl->data); 
		pos+=strlen(gl->data)+1;
	}

	*pos = '\001';

	return ret;
}

void ctcp_reply(struct ctcp_handle *h, ...) 
{
	va_list ap;
	char *msg;
	va_start(ap, h);
	msg = toarg(ap);
	va_end(ap);

	network_send_args(h->network, "NOTICE", h->nick, msg, NULL);
	g_free(msg);
}

void ctcp_send(struct irc_network *n, const char *nick, ...)
{
	va_list ap;
	char *msg;
	va_start(ap, nick);
	msg = toarg(ap);
	va_end(ap);

	network_send_args(n, "PRIVMSG", nick, msg, NULL);
	g_free(msg);
}

static void handle_time(struct ctcp_handle *h, const char **args)
{
	time_t ti = time(NULL);
	char *t, *msg = g_strdup(ctime(&ti));
	t = strchr(msg, '\n');
	if (t)*t = '\0';
	ctcp_reply(h, "TIME", msg, NULL);
	g_free(msg);
}

static void handle_finger(struct ctcp_handle *h, const char **args)
{
	ctcp_reply(h, "FINGER", h->network->external_state->me.fullname, NULL);
}

static void handle_source(struct ctcp_handle *h, const char **args)
{
	ctcp_reply(h, "SOURCE", "www.ctrlproxy.org:pub/ctrlproxy:ctrlproxy-latest.tar.gz", NULL);
}

static void handle_version(struct ctcp_handle *h, const char **args)
{
	char *msg;
#ifndef _WIN32
	struct utsname u;
	uname(&u);
	msg = g_strdup_printf("ctrlproxy:%s:%s %s", ctrlproxy_version(), u.sysname, u.release);
#else
	msg = g_strdup_printf("ctrlproxy:%s:Windows %d.%d", ctrlproxy_version(), _winmajor, _winminor);
#endif
	ctcp_reply(h, "VERSION", msg, NULL);
	g_free(msg);
}

static void handle_ping(struct ctcp_handle *h, const char **args)
{
	ctcp_reply(h, "PING", args[1], NULL);
}

static void handle_clientinfo(struct ctcp_handle *h, const char **args);

static const struct ctcp_handler builtins[] = {
	{ "VERSION", handle_version },
	{ "CLIENTINFO", handle_clientinfo },
	{ "SOURCE", handle_source },
	{ "PING", handle_ping },
	{ "FINGER", handle_finger },
	{ "TIME", handle_time },
	{ "ACTION", NULL },
	{ NULL, NULL }
};

static GList *cmds = NULL;

static void handle_clientinfo(struct ctcp_handle *h, const char **args)
{
	const char * *supported = g_new0(const char *, sizeof(builtins)/sizeof(builtins[0])+1+g_list_length(cmds));
	char *tmp;
	int i;
	GList *gl;

	for (i = 0; builtins[i].name; i++) 
		supported[i] = builtins[i].name;

	for (gl = cmds; gl; gl = gl->next) {
		supported[i] = ((struct ctcp_handler *)gl->data)->name;
		i++;
	}

	supported[i] = NULL;

	tmp = g_strjoinv(" ", (char **)supported);
	ctcp_reply(h, "CLIENTINFO", tmp, NULL);
	g_free(tmp);
	g_free(supported);
}

void ctcp_register_handler(const struct ctcp_handler *h)
{
	cmds = g_list_append(cmds, g_memdup(h, sizeof(*h)));
}

gboolean ctcp_process_request (struct irc_network *n, const struct irc_line *l) 
{
	GList *gl;
	int i;
	char *t;
	char *data;
	char **args;
	gboolean ret = FALSE;
	struct ctcp_handle h;

	h.network = n;
	h.nick = line_get_nick(l);

	data = g_strdup(l->args[2]+1);
	t = strchr(data, '\001');
	if (!t) { 
		g_free(data); 
		network_log(LOG_WARNING, n, "Malformed CTCP request from %s!", h.nick);
		g_free(h.nick);
		return FALSE;
	}
	*t = '\0';

	args = g_strsplit(data, " ", 0);
	for (gl = cmds; gl; gl = gl->next) {
		struct ctcp_handler *hl = gl->data;

		if (!g_strcasecmp(hl->name, args[0])) {
			if (hl->fn != NULL)
				hl->fn(&h, (const char **)args);
			ret = TRUE;
			break;
		}
	}

	for (i = 0; !ret && builtins[i].name; i++) {
		if (!g_strcasecmp(builtins[i].name, args[0])) {
			if (builtins[i].fn != NULL) 
				builtins[i].fn(&h, (const char **)args);
			ret = TRUE;
			break;
		}
	}

	if (!ret) {
		ctcp_reply(&h, "ERRMSG", NULL);
		network_log(LOG_WARNING, n, "Received unknown CTCP request '%s'!", data);
		ret = FALSE;
	}


	g_strfreev(args);
	g_free(data);
	g_free(h.nick);

	return ret;
}

struct irc_network *ctcp_get_network(struct ctcp_handle *h)
{
       return h->network;
}

struct network_nick *ctcp_get_network_nick(struct ctcp_handle *h)
{
       return find_network_nick(h->network->external_state, h->nick);
}

const char *ctcp_get_nick(struct ctcp_handle *h)
{
       return h->nick;
}

