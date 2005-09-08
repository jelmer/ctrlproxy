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
#include "../ctrlproxy.h"

int test_rfccmp(void)
{
	if (str_rfc1459cmp("abcde", "ABCDE") != 0) return -1;
	if (str_rfc1459cmp("abcde~{}", "ABCDE^[]") != 0) return -2;
	if (str_asciicmp("abcde", "ABCDE") != 0) return -3;
	if (str_strictrfc1459cmp("abcde{}", "ABCDE[]") != 0) return -4;
	if (str_strictrfc1459cmp("abcde{}^", "ABCDE[]~") == 0) return -5;
	if (str_strictrfc1459cmp("abcde{}", "abcde{}") != 0) return -6;
	if (str_strictrfc1459cmp("abcde{}^", "abcde{}") == 0) return -7;

	return 0;
}


void torture_init(void)
{
	register_test("TEST-IRCCMP1459", test_rfccmp);
}
