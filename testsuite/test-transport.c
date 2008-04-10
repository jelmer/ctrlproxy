/*
    ircdtorture: an IRC RFC compliancy tester
	(c) 2008 Jelmer Vernooij <jelmer@nl.linux.org>

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
#include <unistd.h>
#include <string.h>
#include <check.h>
#include "transport.h"
#include <ctrlproxy.h>
#include "torture.h"

START_TEST(test_create)
	GIOChannel *ch1, *ch2;
	g_io_channel_pair(&ch1, &ch2);
	irc_transport_new_iochannel(ch1);
END_TEST

Suite *transport_suite()
{
	Suite *s = suite_create("transport");
	TCase *tc_iochannel = tcase_create("iochannel");
	suite_add_tcase(s, tc_iochannel);
	tcase_add_test(tc_iochannel, test_create);
	return s;
}
