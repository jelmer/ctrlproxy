/* 
	ctrlproxy: A modular IRC proxy
	(c) 2002-2003 Jelmer Vernooij <jelmer@nl.linux.org>

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

/* Keep track of certain 'events' for every nick and for total */

#define _GNU_SOURCE
#include "ctrlproxy.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <tdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int traverse_keys(TDB_CONTEXT *tdb_context, TDB_DATA key, TDB_DATA value, void *pattern)
{
	long *ivalue;
	if(!key.dptr) return 0;

	ivalue = (long *)value.dptr;
	printf("%s: %ld\n", key.dptr, *ivalue);
	return 0;
}

int main(int argc, char **argv)
{
	TDB_CONTEXT *tdb_context;

	if(argc < 2) { 
		fprintf(stderr, "No file specified\n");
		return 1;
	}

	tdb_context = tdb_open(argv[1], 0, 0, O_RDONLY, 00700);

	tdb_traverse(tdb_context, traverse_keys, NULL);
	return 0;
}
