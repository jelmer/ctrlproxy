/*
	ctrlproxy: A modular IRC proxy
	help: module for access the help system

	(c) 2006 Jelmer Vernooij <jelmer@nl.linux.org>
	(c) 2004 Wilmer van der Gaast <wilmer@gaast.net>

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

#include "internals.h"
#include "help.h"

#define BUFSIZE 1100

help_t *help_init( help_t **help, const char *helpfile )
{
	int i, buflen = 0;
	help_t *h;
	char *s, *t;
	time_t mtime;
	struct stat stat[1];
	
	*help = h = g_new0 ( help_t, 1 );
	
	h->fd = open( helpfile, O_RDONLY
#ifdef _WIN32
				  | O_BINARY
#endif
				  );
	
	if( h->fd == -1 )
	{
		g_free( h );
		return( *help = NULL );
	}
	
	if( fstat( h->fd, stat ) != 0 )
	{
		g_free( h );
		return( *help = NULL );
	}
	mtime = stat->st_mtime;
	
	s = g_new (char, BUFSIZE + 1 );
	s[BUFSIZE] = 0;
	
	while( ( ( i = read( h->fd, s + buflen, BUFSIZE - buflen ) ) > 0 ) ||
	       ( i == 0 && strstr( s, "\n%\n" ) ) )
	{
		buflen += i;
		memset( s + buflen, 0, BUFSIZE - buflen );
		if( !( t = strstr( s, "\n%\n" ) ) || s[0] != '?' )
		{
			/* FIXME: Clean up */
			*help = NULL;
			g_free( s );
			return( NULL );
		}
		i = strchr( s, '\n' ) - s;
		
		if( h->string )
		{
			h = h->next = g_new0( help_t, 1 );
		}
		h->string = g_new ( char, i );
		
		strncpy( h->string, s + 1, i - 1 );
		h->string[i-1] = 0;
		h->fd = (*help)->fd;
		h->offset.file_offset = lseek( h->fd, 0, SEEK_CUR ) - buflen + i + 1;
		h->length = t - s - i - 1;
		h->mtime = mtime;
		
		buflen -= ( t + 3 - s );
		t = g_strdup( t + 3 );
		g_free( s );
		s = g_renew( char, t, BUFSIZE + 1 );
		s[BUFSIZE] = 0;
	}
	
	g_free( s );
	
	return( *help );
}

char *help_get( help_t **help, const char *string )
{
	time_t mtime;
	struct stat stat[1];
	help_t *h;

	h=*help;	

	while( h )
	{
		if( g_strcasecmp( h->string, string ) == 0 ) break;
		h = h->next;
	}
	if( h && h->length > 0 )
	{
		char *s = g_new( char, h->length + 1 );
		
		if( fstat( h->fd, stat ) != 0 )
		{
			g_free( h );
			*help = NULL;
			return NULL;
		}
		mtime = stat->st_mtime;
		
		if( mtime > h->mtime )
			return NULL;
		
		s[h->length] = 0;
		if( h->fd >= 0 )
		{
			lseek( h->fd, h->offset.file_offset, SEEK_SET );
			read( h->fd, s, h->length );
		}
		else
		{
			strncpy( s, h->offset.mem_offset, h->length );
		}
		return s;
	}
	
	return NULL;
}

void admin_cmd_help(admin_handle h, char **args, void *userdata)
{
	extern GList *admin_commands;
	extern guint longest_command;
	GList *gl;
	char *tmp;
	char **details;
	int i;

	if (args[1]) {
		admin_out(h, "Details for command %s:", args[1]);
		for (gl = admin_commands; gl; gl = gl->next) {
			struct admin_command *cmd = (struct admin_command *)gl->data;
			if(!g_strcasecmp(args[1], cmd->name)) {
				if(cmd->help_details != NULL) {
					details = g_strsplit(cmd->help_details, "\n", 0);
					for(i = 0; details[i] != NULL; i++) {
						admin_out(h, details[i]);
					}
					return;
				} else {
					admin_out(h, "Sorry, no help for %s available", args[1]);
				}
			}
		}
		admin_out(h, "Unknown command");
	} else {
		admin_out(h, "The following commands are available:");
		for (gl = admin_commands; gl; gl = gl->next) {
			struct admin_command *cmd = (struct admin_command *)gl->data;
			if(cmd->help != NULL) {
				tmp = g_strdup_printf("%s%s     %s",cmd->name,g_strnfill(longest_command - strlen(cmd->name),' '),cmd->help);
				admin_out(h, tmp);
				g_free(tmp);
			} else {
				admin_out(h, cmd->name);
			}
		}
	}
}
