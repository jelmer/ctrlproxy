/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2007 Jelmer Vernooij <jelmer@nl.linux.org>

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

#ifndef _CTRLPROXY_LOG_SUPPORT_H_
#define _CTRLPROXY_LOG_SUPPORT_H_

#include <stdio.h>

struct log_file_info {
	FILE *file;
	time_t last_used;
};

struct log_support_context {
	GHashTable *files;
	int num_opened;
};

struct log_support_context *log_support_init(void);
gboolean log_support_write(struct log_support_context *ctx, 
					   	   const char *path,
						   const char *text);
void log_support_writef(struct log_support_context *ctx, 
					   const char *path,
					   const char *fmt, ...);
void free_log_support_context(struct log_support_context *);

#endif /* _CTRLPROXY_LOG_SUPPORT_H_ */
