#include <stdlib.h>
#include <glib.h>
#include <gmodule.h>
#include <check.h>
#include <string.h>
#include <stdio.h>
#include "help.h"

GHashTable *help_build_hash(char *data, gsize len);

static GHashTable *build_hash(const char *data)
{
	return help_build_hash(g_strdup(data), strlen(data));
}

START_TEST(test_help_hash)
{
	GHashTable *h = build_hash("?foo\nbar\n%\n");
	fail_unless(g_hash_table_lookup(h, "foo") != NULL);
	fail_unless(strncmp(g_hash_table_lookup(h, "foo"), "bar\n", 4) == 0);
}
END_TEST

START_TEST(test_help_hash_nopercent)
{
	GHashTable *h = build_hash("?foo\nbar\n");
	fail_unless(h == NULL);
}
END_TEST

START_TEST(test_help_hash_multiple)
{
	GHashTable *h = build_hash("?foo\nbar\n%\n?bla\nbloe\nblie\n%\n");
	fail_unless(g_hash_table_lookup(h, "foo") != NULL);
	fail_unless(strncmp(g_hash_table_lookup(h, "foo"), "bar\n", 4) == 0);
	fail_unless(strncmp(g_hash_table_lookup(h, "bla"), "bloe\nblie\n", 10) == 0);
}
END_TEST

START_TEST(test_help_hash_empty)
{
	GHashTable *h = build_hash("?foo\nbar\n%\n?\nbloe\nblie\n%\n");
	fail_unless(g_hash_table_lookup(h, "foo") != NULL);
	fail_unless(strncmp(g_hash_table_lookup(h, "foo"), "bar\n", 4) == 0);
	fail_unless(strncmp(g_hash_table_lookup(h, ""), "bloe\nblie\n", 10) == 0);
}
END_TEST

START_TEST(test_help_hash_strange)
{
	GHashTable *h = build_hash("barbla");
	fail_unless(h == NULL);
}
END_TEST

START_TEST(test_help_none)
{
	help_t *h;
	h = help_load_file("testsuite/test-help.c");
	fail_if(h == NULL);
}
END_TEST

START_TEST(test_help_nonexistant)
{
	help_t *h;
	h = help_load_file("IDONTEXIST");
	fail_unless(h == NULL);
}
END_TEST

START_TEST(test_help_free)
{
	help_t *h;
	h = help_load_file("testsuite/test-help.c");
	fail_if(h == NULL);
	help_free(h);
}
END_TEST

START_TEST(test_help_get_null)
{
	fail_unless(help_get(NULL, "foo") == NULL);
}
END_TEST

START_TEST(test_help_nonexistent)
{
	help_t *h;
	h = help_load_file("/dev/null");
	fail_unless(help_get(h, "nonexistent") == NULL);
}
END_TEST

Suite *help_suite (void)
{
	Suite *s = suite_create("help");
	TCase *tc_core = tcase_create("Core");
	suite_add_tcase (s, tc_core);
	tcase_add_test (tc_core, test_help_none);
	tcase_add_test (tc_core, test_help_hash);
	tcase_add_test (tc_core, test_help_hash_empty);
	tcase_add_test (tc_core, test_help_hash_multiple);
	tcase_add_test (tc_core, test_help_hash_nopercent);
	tcase_add_test (tc_core, test_help_hash_strange);
	tcase_add_test (tc_core, test_help_nonexistent);
	tcase_add_test (tc_core, test_help_free);
	tcase_add_test (tc_core, test_help_get_null);;
	tcase_add_test (tc_core, test_help_nonexistant);
	return s;
}
