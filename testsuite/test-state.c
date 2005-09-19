
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

gboolean network_nick_set_nick(struct network_nick *, const char *);
gboolean network_nick_set_hostmask(struct network_nick *, const char *);

static int state_join(void)
{
	struct network_state *ns = network_state_init(NULL, "bla", "Gebruikersnaam", "Computernaam");

	if (!ns) return -1;

	if (strcmp(ns->me.nick, "bla") != 0) return -2;
	if (strcmp(ns->me.username, "Gebruikersnaam") != 0) return -3;
	if (strcmp(ns->me.hostname, "Computernaam") != 0) return -4;

	return 0;
}

static int state_set_nick(void)
{
	struct network_nick nn;
	memset(&nn, 0, sizeof(nn));
	
	if (network_nick_set_nick(NULL, "mynick")) return -6;
	
	if (!network_nick_set_nick(&nn, "mynick")) return -5;
	if (strcmp(nn.nick, "mynick") != 0) return -1;
	if (strcmp(nn.hostmask, "mynick!~(null)@(null)") != 0) return -2;
	if (!network_nick_set_nick(&nn, "mynick")) return -6;
	if (strcmp(nn.nick, "mynick") != 0) return -3;
	if (strcmp(nn.hostmask, "mynick!~(null)@(null)") != 0) return -4;

	return 0;
}

static int state_set_hostmask(void)
{
	struct network_nick nn;
	memset(&nn, 0, sizeof(nn));

	if (network_nick_set_hostmask(NULL, "")) return -1;
	if (network_nick_set_hostmask(&nn, NULL)) return -2;
	if (!network_nick_set_hostmask(&nn, "ikke!~uname@uhost")) return -3;
	if (!nn.nick || strcmp(nn.nick, "ikke") != 0) return -4;
	if (!nn.username || strcmp(nn.username, "~uname") != 0) return -5;
	if (!nn.hostname || strcmp(nn.hostname, "uhost") != 0) return -6;
	if (!network_nick_set_hostmask(&nn, "ikke!~uname@uhost")) return -7;

	if (network_nick_set_hostmask(&nn, "ikkeongeldig")) return -8;

	return 0;
}

static int state_marshall_simple(void)
{
	struct network_state *s, *t;
	size_t len1, len2;
	char *data1, *data2;

	s = network_state_init(NULL, "nick", "uname", "uhost");
	data1 = network_state_encode(s, &len1);
	t = network_state_decode(data1, len1, NULL);
	data2 = network_state_encode(s, &len2);

	if (len1 != len2) return -1;

	if (memcmp(data1, data2, len1) != 0) return -2;

	if (strcmp(s->me.nick, t->me.nick) != 0) return -3;
	if (strcmp(s->me.username, t->me.username) != 0) return -3;
	if (strcmp(s->me.hostname, t->me.hostname) != 0) return -4;

	free_network_state(t);

	return 0;
}

void torture_init(void)
{
	register_test("STATE-JOIN", state_join);
	register_test("STATE-SETNICK", state_set_nick);
	register_test("STATE-SETHOSTMASK", state_set_hostmask);
	register_test("MARSHALL-SIMPLE", state_marshall_simple);
}
