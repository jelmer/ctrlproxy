/*
    ircdtorture: an IRC RFC compliancy tester
	(c) 2008 Jelmer Vernooĳ <jelmer@jelmer.uk>

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

#include <stdio.h>
#include <string.h>
#include <check.h>
#include <stdio.h>
#include "transport.h"
#include <ctrlproxy.h>
#include "torture.h"

START_TEST(test_create)
	GIOChannel *ch1, *ch2;
	g_io_channel_pair(&ch1, &ch2);
	irc_transport_new_iochannel(ch1);
END_TEST

START_TEST(test_send)
	GIOChannel *ch1, *ch2;
	struct irc_transport *t;
	int i;
	g_io_channel_pair(&ch1, &ch2);
	g_io_channel_set_encoding(ch1, NULL, NULL);
	g_io_channel_set_flags(ch1, G_IO_FLAG_NONBLOCK, NULL);
	g_io_channel_set_buffered(ch1, FALSE);
	t = irc_transport_new_iochannel(ch1);
	/* saturate the buffer */
	for (i = 0; i < 10000; i++) {
		char buf[20];
		g_snprintf(buf, sizeof(buf), "bar: %d", i);
		fail_unless(transport_send_args(t, NULL, "PRIVMSG", "foo", buf, NULL));
	}
	for (i = 0; i < 10000; i++) {
		char *str;
		char buf[120];
		g_snprintf(buf, sizeof(buf), "PRIVMSG foo :bar: %d\r\n", i);
		g_main_iteration(FALSE);
		g_io_channel_read_line(ch2, &str, NULL, NULL, NULL);
		fail_if(strcmp(str, buf));
	}
END_TEST

Suite *transport_suite()
{
	Suite *s = suite_create("transport");
	TCase *tc_iochannel = tcase_create("iochannel");
	suite_add_tcase(s, tc_iochannel);
	tcase_add_test(tc_iochannel, test_create);
	tcase_add_test(tc_iochannel, test_send);
	return s;
}
