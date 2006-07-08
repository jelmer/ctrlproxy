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
#include "ctrlproxy.h"
#include <check.h>

#define DEFAULT_TIMEOUT 1000

Suite *util_suite(void);
Suite *state_suite(void);
Suite *isupport_suite(void);
Suite *cmp_suite(void);
Suite *line_suite(void);
Suite *parser_suite(void);
Suite *user_suite(void);
Suite *linestack_suite(void);
gboolean init_log(const char *file);

int main (int argc, char **argv)
{
	GOptionContext *pc;
	gboolean no_fork = FALSE;
	gboolean verbose = FALSE;
	gboolean stderr_log = FALSE;
	GOptionEntry options[] = {
		{"no-fork", 'n', 0, G_OPTION_ARG_NONE, &no_fork, "Don't fork" },
		{"stderr", 's', 0, G_OPTION_ARG_NONE, &stderr_log, "Log to stderr", NULL },
		{"verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Be verbose", NULL },
		{ NULL }
	};
	int nf;
	SRunner *sr;

	pc = g_option_context_new("");
	g_option_context_add_main_entries(pc, options, NULL);

	if(!g_option_context_parse(pc, &argc, &argv, NULL))
		return 1;

	g_option_context_free(pc);

	if (stderr_log)
		init_log(NULL);

	sr = srunner_create(util_suite());
	srunner_add_suite(sr, state_suite());
	srunner_add_suite(sr, isupport_suite());
	srunner_add_suite(sr, cmp_suite());
	srunner_add_suite(sr, parser_suite());
	srunner_add_suite(sr, user_suite());
	srunner_add_suite(sr, line_suite());
	srunner_add_suite(sr, linestack_suite());
	if (no_fork)
		srunner_set_fork_status(sr, CK_NOFORK);
	srunner_run_all (sr, verbose?CK_VERBOSE:CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
