
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

int state_join(void)
{
	struct network_state *ns = new_network_state("bla", "Volledigenaam", "Gebruikersnaam");

	if (!ns) return -1;

	if (!strcmp(ns->me.nick, "bla")) return -2;
	if (!strcmp(ns->me.fullname, "Volledigenaam")) return -2;
	if (!strcmp(ns->me.username, "Gebruikersnaam")) return -2;

	return 0;
}

void torture_init(void)
{
	register_test("STATE-JOIN", state_join);
}
