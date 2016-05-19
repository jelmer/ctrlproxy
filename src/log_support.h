/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2007 Jelmer Vernooij <jelmer@jelmer.uk>

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

#ifndef _CTRLPROXY_LOG_SUPPORT_H_
#define _CTRLPROXY_LOG_SUPPORT_H_

#include <stdio.h>

/**
 * Log file information.
 */
struct log_file_info {
	FILE *file;
	time_t last_used;
};

/**
 * Common logging data. Contains a cache of log files that have been
 * written to. Will keep a limited number of file descriptors open,
 * for performance reasons.
 */
struct log_support_context {
	GHashTable *files;
	int num_opened;
};

G_MODULE_EXPORT G_GNUC_MALLOC struct log_support_context *log_support_init(void);
G_MODULE_EXPORT gboolean log_support_write(struct log_support_context *ctx,
					   	   const char *path,
						   const char *text);
G_MODULE_EXPORT G_GNUC_PRINTF(3, 4) void log_support_writef(struct log_support_context *ctx,
					   const char *path,
					   const char *fmt, ...);
G_MODULE_EXPORT void free_log_support_context(struct log_support_context *);
G_MODULE_EXPORT void log_support_reopen(struct log_support_context *);

#endif /* _CTRLPROXY_LOG_SUPPORT_H_ */
