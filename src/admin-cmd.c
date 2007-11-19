/*
	ctrlproxy: A modular IRC proxy
	admin: module for remote administration. 

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

#include "internals.h"
#include <sys/un.h>
#ifdef HAVE_READLINE
#include <readline/readline.h>
#endif

gboolean admin_socket_prompt(const char *config_dir)
{
	char *admin_dir = g_build_filename(config_dir, "admin", NULL);
	int sock = socket(PF_UNIX, SOCK_STREAM, 0);
	GIOChannel *ch;
	struct sockaddr_un un;

	un.sun_family = AF_UNIX;
	strncpy(un.sun_path, admin_dir, sizeof(un.sun_path));

	if (connect(sock, (struct sockaddr *)&un, sizeof(un)) < 0) {
		fprintf(stderr, "unable to connect to %s: %s\n", un.sun_path, strerror(errno));
		g_free(admin_dir);
		return FALSE;
	}

	ch = g_io_channel_unix_new(sock);

	g_io_channel_set_flags(ch, G_IO_FLAG_NONBLOCK, NULL);
	
#ifdef HAVE_READLINE
	while (1) {
		char *data = readline("ctrlproxy> ");
		char *raw;

		if (data == NULL)
			break;
		
		status = g_io_channel_write_chars(ch, data, -1, NULL, &error);
		if (status != G_IO_STATUS_NORMAL) {
			fprintf(stderr, "Error writing to admin socket: %s\n", error->message);
			return FALSE;
		}

		status = g_io_channel_write_chars(ch, "\n", -1, NULL, &error);
		if (status != G_IO_STATUS_NORMAL) {
			fprintf(stderr, "Error writing to admin socket: %s\n", error->message);
			return FALSE;
		}

		g_io_channel_flush(ch, &error);

		g_free(data);

		g_usleep(G_USEC_PER_SEC / 10);

		while (g_io_channel_read_line(ch, &raw, NULL, NULL, &error) == G_IO_STATUS_NORMAL) 
		{
			printf("%s", raw);
		}
	}
#endif
	g_free(admin_dir);

	return TRUE;
}

int main(int argc, char **argv)
{
	char *config_dir = NULL;
	GError *error = NULL;
	GOptionContext *pc;
	GOptionEntry options[] = {
		{"config-dir", 'c', 0, G_OPTION_ARG_STRING, &config_dir, ("Override configuration directory"), ("DIR")},
		{ NULL }
	};

	pc = g_option_context_new("");
	g_option_context_add_main_entries(pc, options, NULL);
	if (!g_option_context_parse(pc, &argc, &argv, &error)) {
		fprintf(stderr, "%s\n", error->message);
		return 1;
	}

	if (config_dir == NULL) 
		config_dir = g_build_filename(g_get_home_dir(), ".ctrlproxy", NULL);

	g_option_context_free(pc);

	if (admin_socket_prompt(config_dir)) 
		return 0;

	return 1;
}
