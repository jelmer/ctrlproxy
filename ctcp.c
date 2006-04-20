/* 
	ctrlproxy: A modular IRC proxy
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

#include "internals.h"
#ifndef _WIN32
#include <sys/utsname.h>
#endif

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

void ctcp_reply(struct network *n, const char *nick, ...) 
{
	va_list ap;
	char *msg;
	va_start(ap, nick);
	msg = toarg(ap);
	va_end(ap);

	network_send_args(n, "NOTICE", nick, msg, NULL);
	g_free(msg);
}

void ctcp_send(struct network *n, const char *nick, ...)
{
	va_list ap;
	char *msg;
	va_start(ap, nick);
	msg = toarg(ap);
	va_end(ap);

	network_send_args(n, "PRIVMSG", nick, msg, NULL);
	g_free(msg);
}

static char *get_ctcp_command(const char *data)
{
	char *tmp;
	char *p;
	g_assert(data[0] == '\001');

	tmp = g_strdup(data+1);

	p = strchr(tmp, ' ');
	if (p) {
		*p = '\0';
		return tmp;
	}

	p = strrchr(tmp, '\001');
	if (p) {
		*p = '\0';
		return tmp;
	}

	g_free(tmp);
	return NULL;
}


static void handle_time(struct network *n, const char *sender, char **args)
{
	time_t ti = time(NULL);
	char *t, *msg = g_strdup(ctime(&ti));
	t = strchr(msg, '\n');
	if(t)*t = '\0';
	ctcp_reply(n, sender, "TIME", msg, NULL);
	g_free(msg);
}

static void handle_finger(struct network *n, const char *sender, char **args)
{
	ctcp_reply(n, sender, "FINGER", n->state->me.fullname, NULL);
}

static void handle_source(struct network *n, const char *sender, char **args)
{
	ctcp_reply(n, sender, "SOURCE", "http://ctrlproxy.vernstok.nl/:releases:ctrlproxy-latest.tar.gz", NULL);
}

static void handle_clientinfo(struct network *n, const char *sender, char **args)
{
	ctcp_reply(n, sender, "CLIENTINFO", "ACTION CLIENTINFO VERSION TIME FINGER SOURCE CLIENTINFO PING", NULL);
}

static void handle_version(struct network *n, const char *sender, char **args)
{
	char *msg;
#ifndef _WIN32
	struct utsname u;
	uname(&u);
	msg = g_strdup_printf("ctrlproxy:%s:%s %s", ctrlproxy_version(), u.sysname, u.release);
#else
	msg = g_strdup_printf("ctrlproxy:%s:Windows %d.%d", ctrlproxy_version(), _winmajor, _winminor);
#endif
	ctcp_reply(n, sender, "VERSION", msg, NULL);
	g_free(msg);
}

static void handle_ping(struct network *n, const char *sender, char **args)
{
	ctcp_reply(n, sender, args[1], NULL);
}

static const struct ctcp_handler builtins[] = {
	{ "PING", handle_ping },
	{ "VERSION", handle_version },
	{ "CLIENTINFO", handle_clientinfo },
	{ "SOURCE", handle_source },
	{ "PING", handle_ping },
	{ "FINGER", handle_finger },
	{ "TIME", handle_time },
	{ NULL, NULL }
};

static GList *cmds = NULL;

void register_ctcp_handler(const struct ctcp_handler *h)
{
	cmds = g_list_append(cmds, h);
}

struct ctcp_request {
	struct client *client;
	char *destination;
	char *command;
};

static GList *ctcp_request_queue = NULL;

gboolean ctcp_process_client_request (struct client *c, struct line *l)
{
	struct ctcp_request *req;
	char *command = get_ctcp_command(l->args[2]);

	if (command == NULL) {
		log_client(NULL, LOG_WARNING, c, "Received mailformed CTCP request");
		return FALSE;
	}

	req = g_new0(struct ctcp_request, 1);

	/* store client and command in table */
	req->client = c;
	if (!is_channelname(l->args[1], &c->network->info))
		req->destination = g_strdup(l->args[1]);
	req->command = command;

	log_client(NULL, LOG_TRACE, c, "Tracking CTCP request '%s' to %s", req->command, req->destination);

	ctcp_request_queue = g_list_append(ctcp_request_queue, req);

	/* send off to server */
	l->is_private = TRUE;
	network_send_line(c->network, c, l);

	return TRUE;
}

gboolean ctcp_process_client_reply (struct client *c, struct line *l)
{
	char *command = get_ctcp_command(l->args[2]);

	if (command == NULL) {
		log_client(NULL, LOG_WARNING, c, "Received mailformed CTCP reply");
		return FALSE;
	}


	log_client(NULL, LOG_WARNING, c, "Received CTCP client reply '%s' from client", command);
	g_free(command);

	return TRUE;
}

gboolean ctcp_process_network_reply (struct network *n, struct line *l) 
{
	GList *gl;
	char *nick;

	char *command = get_ctcp_command(l->args[2]);

	nick = line_get_nick(l);

	if (command == NULL) {
		log_network(NULL, LOG_WARNING, n, "Received mailformed CTCP request from `%s'", nick);
		g_free(nick);
		return FALSE;
	}


	/* look in table */
	/* if found send to specified client, remove entry from table */
	for (gl = ctcp_request_queue; gl; gl = gl->next) {
		struct ctcp_request *req = gl->data;

		if (req->client->network != n)
			continue;

		if (req->destination && strcmp(req->destination, nick) != 0)
			continue;

		if (strcmp(req->command, command) != 0)
			continue;

		client_send_line(req->client, l);

		g_free(req->command);
		g_free(req->destination);
		ctcp_request_queue = g_list_remove(ctcp_request_queue, req);
		g_free(req);
		g_free(nick);
		g_free(command);

		return TRUE;
	}

	/* otherwise, inform user */
	log_network(NULL, LOG_WARNING, n, "Don't know where to send unknown CTCP reply '%s'", command);

	g_free(command);

	return TRUE;
}

gboolean ctcp_process_network_request (struct network *n, struct line *l) 
{
	GList *gl;
	int i;
	char *sender, *t;
	char *data;
	char **args;
	gboolean ret = FALSE;

	sender = line_get_nick(l);

	data = g_strdup(l->args[2]+1);
	t = strchr(data, '\001');
	if(!t) { 
		g_free(data); 
		log_network(NULL, LOG_WARNING, n, "Malformed CTCP request from %s!", sender);
		g_free(sender);
		return FALSE;
	}
	*t = '\0';

	args = g_strsplit(data, " ", 0);

	for (gl = cmds; gl; gl = gl->next) {
		struct ctcp_handler *h = gl->data;

		if (!g_strcasecmp(h->name, args[0])) {
			h->fn(n, sender, args);
			ret = TRUE;
			break;
		}
	}

	for (i = 0; !ret && builtins[i].name; i++) {
		if (!g_strcasecmp(builtins[i].name, args[0])) {
			builtins[i].fn(n, sender, args);
			ret = TRUE;
			break;
		}
	}

	if (!ret) {
		ctcp_reply(n, sender, "ERRMSG", NULL);
		log_network(NULL, LOG_WARNING, n, "Received unknown CTCP request '%s'!", data);
		ret = FALSE;
	}

	g_strfreev(args);
	g_free(data);
	g_free(sender);

	return ret;
}
