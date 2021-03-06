/*
	ctrlproxy: A modular IRC proxy
	admin: module for remote administration.

	(c) 2003-2007 Jelmer Vernooĳ <jelmer@jelmer.uk>

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

gboolean admin_socket_prompt(const char *config_dir, gboolean python)
{
	char *admin_dir = g_build_filename(config_dir, "admin", NULL);
	int sock = socket(PF_UNIX, SOCK_STREAM, 0);
	GIOChannel *ch;
	struct sockaddr_un un;
	GIOStatus status;
	GError *error = NULL;

	un.sun_family = AF_UNIX;
	strncpy(un.sun_path, admin_dir, sizeof(un.sun_path));

	if (connect(sock, (struct sockaddr *)&un, sizeof(un)) < 0) {
		fprintf(stderr, "unable to connect to %s: %s\n", un.sun_path, strerror(errno));
		g_free(admin_dir);
		return FALSE;
	}

	ch = g_io_channel_unix_new(sock);

	g_io_channel_set_flags(ch, G_IO_FLAG_NONBLOCK, NULL);
	
	while (1) {
		char *raw = readline("ctrlproxy> ");
		char *data;

		if (raw == NULL)
			break;

		if (python) {
			data = g_strdup_printf("python %s\n", raw);
		} else {
			data = g_strdup_printf("%s\n", raw);
		}

		g_free(raw);
	
		status = g_io_channel_write_chars(ch, data, -1, NULL, &error);
		if (status != G_IO_STATUS_NORMAL) {
			fprintf(stderr, "Error writing to admin socket: %s\n", error->message);
			if (error != NULL)
				g_error_free(error);
			return FALSE;
		}

		g_io_channel_flush(ch, &error);

		g_free(data);

		/* A bit ugly, but it works.. */
		g_usleep(G_USEC_PER_SEC / 10);

		while (g_io_channel_read_line(ch, &raw, NULL, NULL, &error) == G_IO_STATUS_NORMAL)
		{
			printf("%s", raw);
			g_free(raw);
		}
		if (error != NULL)
			g_error_free(error);
	}
	g_free(admin_dir);

	return TRUE;
}

int main(int argc, char **argv)
{
	char *config_dir = NULL;
	GError *error = NULL;
	int python = 0;
	GOptionContext *pc;
	GOptionEntry options[] = {
		{"config-dir", 'c', 0, G_OPTION_ARG_STRING, &config_dir, ("Override configuration directory"), ("DIR")},
		{"python", 0, 0, G_OPTION_ARG_NONE, &python, ("Python interpreter")},
		{ NULL }
	};

	pc = g_option_context_new("");
	g_option_context_add_main_entries(pc, options, NULL);
	if (!g_option_context_parse(pc, &argc, &argv, &error)) {
		fprintf(stderr, "%s\n", error->message);
		g_error_free(error);
		return 1;
	}

	if (config_dir == NULL)
		config_dir = g_build_filename(g_get_home_dir(), ".ctrlproxy", NULL);

	g_option_context_free(pc);

	if (admin_socket_prompt(config_dir, python))
		return 0;

	return 1;
}
