/*
	ctrlproxy: A modular IRC proxy
	(c) 2003 Jelmer Vernooij <jelmer@nl.linux.org>

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
#include <time.h>
#include "ctrlproxy.h"
#include "../config.h"
#include "admin.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/utsname.h>
#include <dirent.h>
#include <errno.h>
#include <netinet/in.h>
#include <unistd.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "dcc"
#define DCCBUFFERSIZE 1024

static unsigned long ip_to_numeric(struct in_addr a) 
{
	return a.s_addr;
}

static struct in_addr numeric_to_ip(unsigned long r)
{
	struct in_addr a;
	a.s_addr = r;
	return a;
}

static void dcc_list(struct line *l)
{
	char *p = ctrlproxy_path("dcc");
	DIR *d = opendir(p);	
	struct dirent *e;
	free(p);

	if(!d) {
		g_warning(_("Can't open directory '%s'"), p);
		admin_out(l, _("Can't open directory '%s'"), p);
		return;
	}

	while((e = readdir(d))) {
		admin_out(l, "%s", e->d_name);
	}

	closedir(d);
}

static void dcc_del(struct line *l, const char *f)
{
	char *p;
	char *r;
	if(strchr(f, '/')) {
		admin_out(l, _("Invalid filename"));
		return;
	}

	asprintf(&r, "dcc/%s", f);
	p = ctrlproxy_path(r);
	free(r);

	if(unlink(p) != 0) {
		admin_out(l, _("Error occured while deleting %s: %s"), p, strerror(errno));
	} else {
		admin_out(l, _("%s successfully deleted"), p);
	}
	free(p);
}

static gboolean handle_dcc_send(GIOChannel *source, GIOCondition c, gpointer data)
{
	char *file = (char *)data;
	
	if(c & G_IO_IN) {
		int fd = g_io_channel_unix_get_fd(source);
		struct sockaddr_in clientname;
		size_t clisize;
		int new;
		size_t read = 0;
		FILE *f;
		char buf[DCCBUFFERSIZE];
		GError *error = NULL;
		clisize = sizeof(clientname);

		new = accept(fd, &clientname, &clisize);

		g_io_channel_shutdown(source, FALSE, &error);

		if(new < 0) {
			g_warning(_("Error accepting incoming DCC connection: %s"), strerror(errno));
			free(file);
			return FALSE;
		}

		f = fopen(file, "r");
		if(!f) {
			g_warning(_("Couldn't read from '%s': %s"), file, strerror(errno));
			free(file); 
			return FALSE;
		}

		while((read = fread(buf, 1, sizeof(buf), f))) {
			if(send(new, buf, read, 0) < 0) {
				g_warning(_("Error sending file '%s': %s"), file, strerror(errno));
				free(file);
				fclose(f);
				return FALSE;
			}
		}

		free(file);
		close(new);
		fclose(f);
		return FALSE;
	}

	if(c & G_IO_HUP || c & G_IO_ERR) {
		g_warning(_("Error listening for DCC connections"));
		free(file);
		return FALSE;
	}
	
	return TRUE;
}

static gboolean close_dcc_listen(gpointer data)
{
	GError *error = NULL;
	GIOChannel *ioc = (GIOChannel *)data;
	g_message(_("DCC SEND timed out, removing...\n"));
	g_io_channel_shutdown(ioc, FALSE, &error);
	return FALSE;
}

static int dcc_start_listen(struct in_addr *addr, int *port, const char *file)
{
	struct sockaddr_in name;
	int sock;
	size_t namesize;
	GIOChannel *ioc;

	sock = socket(PF_INET, SOCK_STREAM, 0);
	if(sock < 0) {
		g_warning(_("Error creating socket: %s"), strerror(errno));
		return -1;
	}

	name.sin_family = AF_INET;
	name.sin_port = htons(0);
	name.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(sock, (struct sockaddr *)&name, sizeof(name)) < 0) {
		g_warning(_("Unable to bind: %s"), strerror(errno));
		return -1;
	}

	if(listen(sock, 1) < 0) {
		g_warning(_("Unable to listen: %s"), strerror(errno));
		return -1;
	}

	namesize = sizeof(name);
	if(getsockname(sock, &name, &namesize) < 0) {
		g_warning(_("Unable to get socket name: %s"), strerror(errno));
		return -1;
	}
	*addr = name.sin_addr;
	*port = name.sin_port;

	ioc = g_io_channel_unix_new(sock);
	g_io_add_watch(ioc, G_IO_IN | G_IO_ERR | G_IO_HUP, handle_dcc_send, strdup(file));

	g_timeout_add(300 * 1000, close_dcc_listen, ioc);
	return 0;
}

static void dcc_send(struct line *l, const char *f)
{
	char *r;
	char *s;
	char *p;
	struct in_addr addr;
	int port;
	struct stat lstat;
	
	if(strchr(f, '/')) {
		admin_out(l, _("Invalid filename"));
		return;
	}

	asprintf(&r, "dcc/%s", f);
	p = ctrlproxy_path(r);
	free(r);

	if(dcc_start_listen(&addr, &port, p) < 0) {
		g_warning(_("Unable to listen"));
		return;
	}

	if(stat(p, &lstat) < 0) {
		g_warning(_("Unable to stat '%s': %s"), p, strerror(errno));
		free(p);
		return;
	}
	
	asprintf(&s, "\001DCC SEND %s %lu %d %lu", p, ip_to_numeric(addr), port, lstat.st_size );
	free(p);
	irc_send_args(l->client->incoming, "PRIVMSG", s, NULL);
	free(s);
}

static void dcc_command(char **args, struct line *l)
{
	if(!strcasecmp(args[2], "GET")) {
		dcc_send(l, args[3]);
	} else if(!strcasecmp(args[2], "LIST")) {
		dcc_list(l);
	} else if(!strcasecmp(args[2], "DEL")) {
		dcc_del(l, args[3]);
	} else {
		admin_out(l, _("Unknown subcommand '%s'"), args[2]);
	}
}

static gboolean handle_dcc_receive(GIOChannel *i, GIOCondition c, gpointer data)
{
	FILE *f = (FILE *)data;
	gsize read = 0;
	GError *error = NULL;
	char buf[DCCBUFFERSIZE];

	if(c & G_IO_HUP) {
		g_warning(_("DCC Connection closed"));
		fclose(f);
		return FALSE;
	}

	if(c & G_IO_ERR) {
		g_warning(_("Error occured while receiving data over DCC"));
		fclose(f);
		return FALSE;
	}
	
	GIOStatus status = g_io_channel_read_chars(i, buf, sizeof(buf), &read, &error);

	if(status == G_IO_STATUS_NORMAL) {
		fwrite(buf, 1, read, f);
	}
	return TRUE;
}

static void dcc_start_receive(struct in_addr a, int port, char *fname, size_t size)
{
	GIOChannel *io;
	int sock;
	struct sockaddr_in addr;
	char *newpath, *base;
	FILE *fd;
	
	sock = socket(PF_INET, SOCK_STREAM, 0);
	if(sock < 0) {
		g_warning(_("Unable to create socket: %s"), strerror(errno));
		return;
	}

	addr.sin_addr = a;
	addr.sin_port = port;

	if(connect(sock, &addr, sizeof(addr)) < 0) {
		g_warning(_("Unable to connect to host: %s"), strerror(errno));
		return;
	}
	
	io = g_io_channel_unix_new(sock);
	if(strrchr(fname, '/'))fname = strrchr(fname, '/');
	asprintf(&base, "dcc/%s", fname);
	newpath = ctrlproxy_path(base);
	free(base);
	fd = fopen(newpath, "w+");
	free(newpath);
	if(!fd) {
		g_warning(_("Can't open file for %s: %s"), fname, strerror(errno));
		return;
	}
	g_io_add_watch(io, G_IO_IN | G_IO_ERR | G_IO_HUP, handle_dcc_receive, fd);
}

static gboolean mhandle_data(struct line *l)
{
	char *p, *pe, **cargs;
	size_t size = 0;
	int port = 0;
	struct in_addr addr;
	if(l->direction == TO_SERVER || l->args[2][0] != '\001') return TRUE;

	p = strdup(l->args[2]+1);

	pe = strchr(p, '\001');
	if(!pe) {
		g_warning(_("Invalidly formatted CTCP request received"));
		free(p);
		return TRUE;
	}

	*pe = '\0';

	cargs = g_strsplit(p, " ", 0);

	/* DCC SEND */
	if(cargs[0] && cargs[1] && 
	   !strcasecmp(cargs[0], "DCC") && !strcasecmp(cargs[1], "SEND")) {
		/* FIXME: Rules for what data to accept */

		addr = numeric_to_ip(atol(cargs[3]));
		port = atol(cargs[4]);
		if(cargs[5])size = atol(cargs[5]);

		dcc_start_receive(addr, port, cargs[2], size);
	}

	g_strfreev(cargs);

	return TRUE;
}

gboolean fini_plugin(struct plugin *p)
{
	del_filter_ex("client", mhandle_data);
	unregister_admin_command("DCC");
	return TRUE;
}

const char name_plugin[] = "dcc";

gboolean init_plugin(struct plugin *p) 
{
	if(!plugin_loaded("admin")) {
		g_warning(_("admin module required for dcc module. Please load it first"));
		return FALSE;
	}
	register_admin_command("DCC", dcc_command, NULL, NULL);
	add_filter_ex("dcc", mhandle_data, "client", 1000);
	return TRUE;
}
