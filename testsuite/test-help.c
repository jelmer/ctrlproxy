#include <stdlib.h>
#include <glib.h>
#include <gmodule.h>
#include <check.h>
#include <string.h>
#include <stdio.h>
#include "help.h"

START_TEST(test_help_none)
	help_t *h, *r;
	r = help_init(&h, "/dev/null");
	fail_if(r == NULL);
	fail_if(r != h);
END_TEST

START_TEST(test_help_nonexistent)
	help_t *h, *r;
	r = help_init(&h, "/dev/null");
	fail_unless(help_get(&h, "nonexistent") == NULL);
END_TEST

Suite *help_suite (void)
{
	Suite *s = suite_create("Help");
	TCase *tc_core = tcase_create("Core");
	suite_add_tcase (s, tc_core);
	tcase_add_test (tc_core, test_help_none);
	tcase_add_test (tc_core, test_help_nonexistent);
	return s;
}
