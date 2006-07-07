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

int main (void)
{
	int nf;
	SRunner *sr = srunner_create(util_suite());
	srunner_add_suite(sr, state_suite());
	srunner_add_suite(sr, isupport_suite());
	srunner_add_suite(sr, cmp_suite());
	srunner_add_suite(sr, parser_suite());
	srunner_add_suite(sr, user_suite());
	srunner_add_suite(sr, line_suite());
	srunner_run_all (sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
