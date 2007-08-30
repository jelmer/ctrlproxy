/* 
	ctrlproxy: A modular IRC proxy
	(c) 2003-2007 Jelmer Vernooij <jelmer@nl.linux.org>

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
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "nickserv.h"
#include "irc.h"


const char *nickserv_find_nick(struct network *n, const char *nick)
{
	GList *gl;
	for (gl = n->global->nickserv_nicks; gl; gl = gl->next) {
		struct nickserv_entry *e = gl->data;

		if (g_strcasecmp(e->nick, nick)) 
			continue;

		if (!e->network) return e->pass;
		if (!g_strcasecmp(e->network, n->info.name)) return e->pass;
	}
	
	return NULL;
}

const char *nickserv_nick(struct network *n)
{
	return "NickServ";
}

void nickserv_identify_me(struct network *network, char *nick)
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
		log_network(LOG_INFO, network, "Sending password for %s", nickserv_n);
		network_send_args(network, "PRIVMSG", nickserv_n, raw, NULL);
		g_free(raw);
	} else {
		log_network(LOG_INFO, network, "No password known for `%s'", nick);
	}
}

static gboolean log_data(struct network *n, const struct line *l, enum data_direction dir, void *userdata) 
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
			struct nickserv_entry *e = NULL;
			GList *gl;
			char *newpass = g_strdup(l->args[2] + strlen("IDENTIFY "));
		
			for (gl = n->global->nickserv_nicks; gl; gl = gl->next) {
				e = gl->data;

				if (e->network && !g_strcasecmp(e->network, n->info.name) && 
					!g_strcasecmp(e->nick, n->state->me.nick)) {
					break;		
				}

				if (!e->network && !g_strcasecmp(e->nick, n->state->me.nick) &&
					!g_strcasecmp(e->pass, newpass)) {
					break;
				}
			}

			if (!gl) {
				e = g_new0(struct nickserv_entry, 1);
				e->nick = g_strdup(n->state->me.nick);
				e->network = g_strdup(n->info.name);
				n->global->nickserv_nicks = g_list_prepend(n->global->nickserv_nicks, e);
			}

			if (e->pass == NULL || 
				strcmp(e->pass, newpass) != 0) {
				e->pass = g_strdup(newpass);
				log_network(LOG_INFO, n, "Caching password for nick %s", e->nick);
			} 

			g_free(newpass);
	}

	/* If we receive a nick-already-in-use message, ghost the current user */
	if (dir == FROM_SERVER && atol(l->args[0]) == ERR_NICKNAMEINUSE) {
		const char *pass = nickserv_find_nick(n, nickattempt);
		if (nickattempt && pass) {
			const char *nickserv_n = nickserv_nick(n);
			char *raw;
			
			log_network(LOG_INFO, n, "Ghosting current user using '%s'", nickattempt);

			raw = g_strdup_printf("GHOST %s %s", nickattempt, pass);
			network_send_args(n, "PRIVMSG", nickserv_n, raw, NULL);
			g_free(raw);
			network_send_args(n, "NICK", nickattempt, NULL);
		}
	}

	return TRUE;
}

gboolean nickserv_write_file(GList *nicks, const char *filename)
{
	GList *gl;
	int fd;

	fd = open(filename, O_WRONLY | O_CREAT, 0600);

    if (fd == -1) {
		log_global(LOG_WARNING, "Unable to write nickserv file `%s': %s", filename, strerror(errno));
        return FALSE;
    }

	for (gl = nicks; gl; gl = gl->next) {
		struct nickserv_entry *n = gl->data;
        char *line;
        
        line = g_strdup_printf("%s\t%s\t%s\n", n->nick, n->pass, n->network?n->network:"*");
		if (write(fd, line, strlen(line)) < 0) {
			log_global(LOG_WARNING, "error writing line `%s': %s", line, strerror(errno));
			g_free(line);
			close(fd);
			return FALSE;
		}

        g_free(line);
	}
    
    close(fd);

	return TRUE;
}

gboolean nickserv_save(struct global *global, const char *dir)
{
    char *filename = g_build_filename(dir, "nickserv", NULL);
	gboolean ret;

	ret = nickserv_write_file(global->nickserv_nicks, filename);

    g_free(filename);

	return ret;
}

gboolean nickserv_read_file(const char *filename, GList **nicks)
{
    GIOChannel *gio;
    char *ret;
    gsize nr, term;

    gio = g_io_channel_new_file(filename, "r", NULL);

    if (gio == NULL) {
        return FALSE;
    }

    while (G_IO_STATUS_NORMAL == g_io_channel_read_line(gio, &ret, &nr, &term, NULL))
    {
        char **parts; 
		struct nickserv_entry *e;

		ret[term] = '\0';

        parts = g_strsplit(ret, "\t", 3);
        g_free(ret);

		if (!parts[0] || !parts[1]) {
			g_strfreev(parts);
			continue;
		}
			
		e = g_new0(struct nickserv_entry, 1);
		e->nick = parts[0];
		e->pass = parts[1];
		if (!parts[2] || !strcmp(parts[2], "*")) {
			e->network = NULL;
			g_free(parts[2]);
		} else {
			e->network = parts[2];
		}
	
		*nicks = g_list_append(*nicks, e);   
        g_free(parts);
    }

	g_io_channel_shutdown(gio, TRUE, NULL);
	g_io_channel_unref(gio);

	return TRUE;
}

gboolean nickserv_load(struct global *global)
{
	gboolean ret;
    char *filename = g_build_filename(global->config->config_dir, "nickserv", 
									  NULL);
	ret = nickserv_read_file(filename, &global->nickserv_nicks);
	g_free(filename);

	return TRUE;
}

void init_nickserv(void)
{
	add_server_filter("nickserv", log_data, NULL, 1);
}
