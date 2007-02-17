/*
    torture: test suite for ctrlproxy
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <glib.h>
#include <gmodule.h>
#include "internals.h"
#include <check.h>
#include <sys/socket.h>

#define DEFAULT_TIMEOUT 1000

static char test_dir[PATH_MAX];

Suite *tls_suite(void);
Suite *util_suite(void);
Suite *state_suite(void);
Suite *isupport_suite(void);
Suite *cmp_suite(void);
Suite *client_suite(void);
Suite *admin_suite(void);
Suite *network_suite(void);
Suite *line_suite(void);
Suite *parser_suite(void);
Suite *user_suite(void);
Suite *linestack_suite(void);
Suite *redirect_suite(void);
Suite *networkinfo_suite(void);
Suite *ctcp_suite(void);
Suite *help_suite(void);
Suite *nickserv_suite(void);
gboolean init_log(const char *file);

char *torture_tempfile(const char *path)
{
	return g_build_filename(path, test_dir, path, NULL);
}

struct network *dummy_network(void)
{
	struct network_config nc = {
		.name = "test"
	};
	struct network *n;
	n = load_network(NULL, &nc);
	
	return n;
}

struct global *torture_global(const char *name)
{
	char *config_dir = g_build_filename(test_dir, name, NULL);
	struct global *g;

	g = init_global();
	g->config = init_configuration();
	g_assert(g != NULL);
	g->config->config_dir = g_strdup(config_dir);
	save_configuration(g->config, config_dir);

	free_global(g);

	g = load_global(config_dir);
	g_assert(g != NULL);

	g_free(config_dir);
	return g;
}

gboolean g_io_channel_pair(GIOChannel **ch1, GIOChannel **ch2)
{
	int sock[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNIX, sock) < 0) {
		perror("socketpair");
		return FALSE;
	}

	*ch1 = g_io_channel_unix_new(sock[0]);
	*ch2 = g_io_channel_unix_new(sock[1]);
	return TRUE;
}
extern enum log_level current_log_level;

int main (int argc, char **argv)
{
	GOptionContext *pc;
	gboolean no_fork = FALSE;
	gboolean verbose = FALSE;
	gboolean stderr_log = FALSE;
	gboolean trace = FALSE;
	GOptionEntry options[] = {
		{"no-fork", 'n', 0, G_OPTION_ARG_NONE, &no_fork, "Don't fork" },
		{"stderr", 's', 0, G_OPTION_ARG_NONE, &stderr_log, "Log to stderr", NULL },
		{"verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Be verbose", NULL },
		{"trace", 't', 0, G_OPTION_ARG_NONE, &trace, "Trace data", NULL },
		{ NULL }
	};
	int nf;
	SRunner *sr;
	int i;

	pc = g_option_context_new("");
	g_option_context_add_main_entries(pc, options, NULL);

	if(!g_option_context_parse(pc, &argc, &argv, NULL))
		return 1;

	g_option_context_free(pc);

	if (stderr_log)
		init_log(NULL);

	stderr_log |= trace;

	if (trace)
		current_log_level = LOG_TRACE;

	for (i = 0; i < 1000; i++) {
		snprintf(test_dir, sizeof(test_dir), "test-%d", i);
		if (mkdir(test_dir, 0755) == 0)
			break;
	}

	sr = srunner_create(util_suite());
	srunner_add_suite(sr, state_suite());
	srunner_add_suite(sr, isupport_suite());
	srunner_add_suite(sr, cmp_suite());
	srunner_add_suite(sr, client_suite());
	srunner_add_suite(sr, network_suite());
	srunner_add_suite(sr, parser_suite());
	srunner_add_suite(sr, user_suite());
	srunner_add_suite(sr, line_suite());
	srunner_add_suite(sr, linestack_suite());
	srunner_add_suite(sr, tls_suite());
	srunner_add_suite(sr, redirect_suite());
	srunner_add_suite(sr, networkinfo_suite());
	srunner_add_suite(sr, admin_suite());
	srunner_add_suite(sr, ctcp_suite());
	srunner_add_suite(sr, help_suite());
	srunner_add_suite(sr, nickserv_suite());
	if (no_fork)
		srunner_set_fork_status(sr, CK_NOFORK);
	srunner_run_all (sr, verbose?CK_VERBOSE:CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
