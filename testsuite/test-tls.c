/*
    ircdtorture: an IRC RFC compliancy tester
	(c) 2006 Jelmer Vernooij <jelmer@nl.linux.org>

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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <check.h>
#include "ctrlproxy.h"
#include "torture.h"
#include "ssl.h"
#include "config.h"

#ifdef HAVE_GNUTLS
START_TEST(test_tlscert)
	char *keyfile, *cafile, *certfile;

	keyfile = torture_tempfile("keyfile.pm");
	cafile = torture_tempfile("cafile.pm");
	certfile = torture_tempfile("certfile.pm");

	ssl_cert_generate(keyfile, certfile, cafile);

	fail_unless(g_file_test(keyfile, G_FILE_TEST_EXISTS));
	fail_unless(g_file_test(cafile, G_FILE_TEST_EXISTS));
	fail_unless(g_file_test(certfile, G_FILE_TEST_EXISTS));
END_TEST
#endif

Suite *tls_suite()
{
	Suite *s = suite_create("tls");
	TCase *tc_core = tcase_create("core");
	suite_add_tcase(s, tc_core);
#ifdef HAVE_GNUTLS
#if 0
	tcase_add_test(tc_core, test_tlscert);
#endif
#endif
	return s;
}
