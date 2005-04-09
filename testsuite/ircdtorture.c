/*
    ircdtorture: an IRC compliancy tester
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_POPT_H
#include <popt.h>
#endif

int main(int argc, const char *argv[])
{
	const char *inetd = NULL;
	const char *tcp = NULL;
#ifdef HAVE_POPT_H
	int c;
	poptContext pc;
	struct poptOption options[] = {
		POPT_AUTOHELP
		{"tcp", 't', POPT_ARG_STRING, &tcp, 't', "Connection to specified host", "HOST"},
		{"inetd", 'i', POPT_ARG_STRING, &inetd, 'i', "Test PROGRAM inetd-style", "PROGRAM"},
		{"version", 'v', POPT_ARG_NONE, NULL, 'v', "Show version information"},
		POPT_TABLEEND
	};
#endif

#ifdef HAVE_POPT_H
	pc = poptGetContext(argv[0], argc, argv, options, 0);

	while((c = poptGetNextOpt(pc)) >= 0) {
		switch(c) {
		case 'v':
			printf("ircdtorture %s\n", PACKAGE_VERSION);
			printf(("(c) 2005 Jelmer Vernooij et al. <jelmer@nl.linux.org>\n"));
			return 0;
		}
	}
#endif

	return 0;
}
