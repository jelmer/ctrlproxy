/*
    ircdtorture: an IRC RFC compliance tester
	(c) 2007 Jelmer Vernooĳ <jelmer@jelmer.uk>

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
#include "ctrlproxy.h"
#include "torture.h"

Suite *ctcp_suite()
{
	Suite *s = suite_create("ctcp");
	TCase *tc_core = tcase_create("core");
	suite_add_tcase(s, tc_core);
	return s;
}
