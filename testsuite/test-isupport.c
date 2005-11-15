
/*
    ircdtorture: an IRC RFC compliancy tester
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

#include "torture.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "ctrlproxy.h"

static int isupport_isprefix(void)
{
	if (!is_prefix('@', NULL)) return -1;
	if (is_prefix('a', NULL)) return -2;

	return 0;
}

static int isupport_ischannelname(void)
{
	if (!is_channelname("#bla", NULL)) return -1;
	if (!is_channelname("&bla", NULL)) return -2;
	if (is_channelname("bla", NULL)) return -3;

	return 0;
}

static int isupport_prefixbymode(void)
{
	if (get_prefix_by_mode('o',NULL) != '@') return -1;
	if (get_prefix_by_mode('v',NULL) != '+') return -2;
	if (get_prefix_by_mode('x',NULL) != ' ') return -3;

	return 0;
}

void torture_init(void)
{
	register_test("ISUPPORT-ISPREFIX", isupport_isprefix);
	register_test("ISUPPORT-ISCHANNELNAME", isupport_ischannelname);
	register_test("ISUPPORT-PREFIXBYMODE", isupport_prefixbymode);
}
