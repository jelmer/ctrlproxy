/* 
	ctrlproxy: A modular IRC proxy
	(c) 2003-2007 Jelmer Vernooij <jelmer@nl.linux.org>

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

#include "ctrlproxy.h"
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "keyfile.h"
#include "irc.h"

#define NICKSERV_FILE_HEADER "# This file contains passwords for NickServ\n" \
	"# It should contain one entry per line, each entry consisting of: \n" \
	"# a nick name, password and network name, separated by tabs.\n" \
	"#\n"

const char *nickserv_find_nick(struct irc_network *n, const char *nick)
{
	GList *gl;
	for (gl = n->global->nickserv_nicks; gl; gl = gl->next) {
		struct keyfile_entry *e = gl->data;

		if (g_strcasecmp(e->nick, nick)) 
			continue;

		if (!e->network) return e->pass;
		if (!g_strcasecmp(e->network, n->name)) return e->pass;
	}
	
	return NULL;
}

const char *nickserv_nick(struct irc_network *n)
{
	return "NickServ";
}

void nickserv_identify_me(struct irc_network *network, char *nick)
{
	const char *pass;

	/* Don't try to identify if we're already identified */
	/* FIXME: Apparently, +e indicates being registered on Freenode,
	 * +R is only used on OFTC */
	if (network->state->me.modes['R']) 
		return;
	
	pass = nickserv_find_nick(network, nick);
	
	if (pass) {
		const char *nickserv_n = nickserv_nick(network);
		char *raw;
		raw = g_strdup_printf("IDENTIFY %s", pass);
		network_log(LOG_INFO, network, "Sending password for %s", nickserv_n);
		network_send_args(network, "PRIVMSG", nickserv_n, raw, NULL);
		g_free(raw);
	} else {
		network_log(LOG_INFO, network, "No password known for `%s'", nick);
	}
}

static void cache_nickserv_pass(struct irc_network *n, const char *newpass)
{
	struct keyfile_entry *e = NULL;
	GList *gl;

	if (!n->global->config->learn_nickserv)
		return;

	for (gl = n->global->nickserv_nicks; gl; gl = gl->next) {
		e = gl->data;

		if (e->network && !g_strcasecmp(e->network, n->name) && 
			!g_strcasecmp(e->nick, n->state->me.nick)) {
			break;		
		}

		if (!e->network && !g_strcasecmp(e->nick, n->state->me.nick) &&
			!g_strcasecmp(e->pass, newpass)) {
			break;
		}
	}

	if (gl == NULL) {
		e = g_new0(struct keyfile_entry, 1);
		e->nick = g_strdup(n->state->me.nick);
		e->network = g_strdup(n->name);
		n->global->nickserv_nicks = g_list_prepend(n->global->nickserv_nicks, e);
	}

	if (e->pass == NULL || strcmp(e->pass, newpass) != 0) {
		e->pass = g_strdup(newpass);
		network_log(LOG_INFO, n, "Caching password for nick %s", e->nick);
	} 
}

static gboolean log_data(struct irc_network *n, const struct irc_line *l, enum data_direction dir, void *userdata) 
{
	static char *nickattempt = NULL;

	/* User has changed his/her nick. Check whether this nick needs to be identified */
	if (dir == FROM_SERVER && !g_strcasecmp(l->args[0], "NICK") &&
	   nickattempt && !g_strcasecmp(nickattempt, l->args[1])) {
		nickserv_identify_me(n, l->args[1]);
	}

	/* Keep track of the last nick that the user tried to take */
	if (dir == TO_SERVER && !g_strcasecmp(l->args[0], "NICK")) {
		if (nickattempt) g_free(nickattempt);
		nickattempt = g_strdup(l->args[1]);
	}

	if (dir == TO_SERVER && 
		(!g_strcasecmp(l->args[0], "PRIVMSG") || !g_strcasecmp(l->args[0], "NOTICE")) &&
		(!g_strcasecmp(l->args[1], nickserv_nick(n)) && !g_strncasecmp(l->args[2], "IDENTIFY ", strlen("IDENTIFY ")))) {
			cache_nickserv_pass(n, l->args[2] + strlen("IDENTIFY "));
	}

	if (dir == TO_SERVER && 
		!g_strcasecmp(l->args[0], "NS") &&
		!g_strcasecmp(l->args[1], "IDENTIFY")) {
		cache_nickserv_pass(n, l->args[2]);
	}

	/* If we receive a nick-already-in-use message, ghost the current user */
	if (dir == FROM_SERVER && atol(l->args[0]) == ERR_NICKNAMEINUSE) {
		const char *pass = nickserv_find_nick(n, nickattempt);
		if (nickattempt && pass) {
			const char *nickserv_n = nickserv_nick(n);
			char *raw;
			
			network_log(LOG_INFO, n, "Ghosting current user using '%s'", nickattempt);

			raw = g_strdup_printf("GHOST %s %s", nickattempt, pass);
			network_send_args(n, "PRIVMSG", nickserv_n, raw, NULL);
			g_free(raw);
			network_send_args(n, "NICK", nickattempt, NULL);
		}
	}

	return TRUE;
}

gboolean nickserv_save(struct global *global, const char *dir)
{
    char *filename = g_build_filename(dir, "nickserv", NULL);
	gboolean ret;

	if (global->nickserv_nicks == NULL) {
		if (unlink(filename) == 0)
			ret = TRUE;
		else if (errno == ENOENT)
			ret = TRUE;
		else
			ret = FALSE;
	} else 
		ret = keyfile_write_file(global->nickserv_nicks, NICKSERV_FILE_HEADER, filename);

    g_free(filename);

	return ret;
}

gboolean nickserv_load(struct global *global)
{
	gboolean ret;
    char *filename;
	
	filename = g_build_filename(global->config->config_dir, "nickserv", 
									  NULL);
	ret = keyfile_read_file(filename, '#', &global->nickserv_nicks);
	g_free(filename);

	return TRUE;
}

void init_nickserv(void)
{
	add_server_filter("nickserv", log_data, NULL, 1);
}
