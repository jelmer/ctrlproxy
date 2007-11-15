/* 
	ctrlproxy: A modular IRC proxy
	(c) 2002-2003 Jelmer Vernooij <jelmer@nl.linux.org>

	g_mkdir_with_parents() imported from libglib and 
	(C) Red Hat.

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

#include "internals.h"
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <glib/gstdio.h>

char *list_make_string(GList *list)
{
	size_t len = 20;
	char *ret;
	GList *gl;

	/* First, calculate the length */
	for(gl = list; gl; gl = gl->next) len+=strlen(gl->data)+1;

	ret = g_new(char,len);
	ret[0] = '\0';

	for(gl = list; gl; gl = gl->next) 
	{ 
		strncat(ret, gl->data, len);
		if (gl->next) strncat(ret, " ", len); 
	}

	return ret;
}

static inline int str_cmphelper(const char *a, const char *b, char sh, char sl, char eh, char el)
{
	int i;
	char h,l;
	for (i = 0; a[i] && b[i]; i++) {
		if (a[i] == b[i]) continue;
		l = (a[i]>b[i]?b[i]:a[i]);
		h = (a[i]>b[i]?a[i]:b[i]);

		if (h < sh || h > eh || l < sl || l > el) 
			break;

		if (h-sh != l-sl)
			break;
	}

	return a[i]-b[i];
}

int str_asciicmp(const char *a, const char *b)
{
	g_assert(a != NULL);
	g_assert(b != NULL);
	return str_cmphelper(a, b, 97, 65, 122, 90);
}

int str_strictrfc1459cmp(const char *a, const char *b)
{
	g_assert(a != NULL);
	g_assert(b != NULL);
	return str_cmphelper(a, b, 97, 65, 125, 93);
}


int str_rfc1459cmp(const char *a, const char *b)
{
	g_assert(a != NULL);
	g_assert(b != NULL);
	return str_cmphelper(a, b, 97, 65, 126, 94);
}

char *g_io_channel_ip_get_description(GIOChannel *ch)
{
	socklen_t len = sizeof(struct sockaddr_storage);
	struct sockaddr *sa = g_malloc(len);
	char hostname[NI_MAXHOST];
	char service[NI_MAXSERV];
	char *description = NULL;
	int fd = g_io_channel_unix_get_fd(ch);

	if (getpeername (fd, sa, &len) == 0 &&
		getnameinfo(sa, len, hostname, sizeof(hostname),
					service, sizeof(service), NI_NOFQDN | NI_NUMERICSERV) == 0) {

		description = g_strdup_printf("%s:%s", hostname, service);
	}

	g_free(sa);
	return description;
}

gboolean    rep_g_file_get_contents         (const gchar *filename,
                                             gchar **contents,
                                             gsize *length,
                                             GError **error)
{
	int fd;	
	struct stat sbuf;
	char *p;

	fd = open(filename, O_RDONLY);
	if (fd == -1) {
		g_set_error(error, G_FILE_ERROR, 
				g_file_error_from_errno (errno),
				"opening file %s", filename);
		return FALSE;
	}

	if (fstat(fd, &sbuf) != 0) {
		g_set_error(error, G_FILE_ERROR, 
				g_file_error_from_errno (errno),
				"statting file %s", filename);
		return FALSE;
	}

	p = (char *)g_malloc(sbuf.st_size+1);
	if (!p) {
		g_set_error(error, G_FILE_ERROR, 
					g_file_error_from_errno (errno),
					"allocating file %s", filename);
		return FALSE;
	}

	if (read(fd, p, sbuf.st_size) != sbuf.st_size) {
		g_set_error(error, G_FILE_ERROR, 
					g_file_error_from_errno (errno),
					"short read %s", filename);
		g_free(p);
		return FALSE;
	}
	p[sbuf.st_size] = 0;

	if (length != NULL) 
		*length = sbuf.st_size;
	*contents = p;

	close(fd);

	return TRUE;
}

gboolean    rep_g_file_set_contents         (const gchar *filename,
                                             const gchar *contents,
                                             gssize length,
                                             GError **error)
{
	int fd, ret;

	fd = open(filename, O_CREAT | O_TRUNC | O_WRONLY, 0644);
	if (fd < 0) {
		g_set_error(error, G_FILE_ERROR, 
				g_file_error_from_errno (errno),
				"opening file %s", filename);
		return FALSE;
	}

	if (length == -1) 
		length = strlen(contents);

	ret = write(fd, contents, length);

	if (ret < 0) {
		g_set_error(error, G_FILE_ERROR, 
				g_file_error_from_errno (errno),
				"writing file %s", filename);
		return FALSE;
	} else if (ret != length) {
		g_set_error(error, G_FILE_ERROR, 
				g_file_error_from_errno (errno),
				"unexpected number of bytes written: %d", ret);
		return FALSE;
	}

	close(fd);

	return TRUE;
}

/**
 * g_mkdir_with_parents:
 * @pathname: a pathname in the GLib file name encoding
 * @mode: permissions to use for newly created directories
 *
 * Create a directory if it doesn't already exist. Create intermediate
 * parent directories as needed, too.
 *
 * Returns: 0 if the directory already exists, or was successfully
 * created. Returns -1 if an error occurred, with errno set.
 *
 * Since: 2.8
 */
int
rep_g_mkdir_with_parents (const gchar *pathname,
		      int          mode)
{
  gchar *fn, *p;

  if (pathname == NULL || *pathname == '\0')
    {
      errno = EINVAL;
      return -1;
    }

  fn = g_strdup (pathname);

  if (g_path_is_absolute (fn))
    p = (gchar *) g_path_skip_root (fn);
  else
    p = fn;

  do
    {
      while (*p && !G_IS_DIR_SEPARATOR (*p))
	p++;
      
      if (!*p)
	p = NULL;
      else
	*p = '\0';
      
      if (!g_file_test (fn, G_FILE_TEST_EXISTS))
	{
	  if (g_mkdir (fn, mode) == -1)
	    {
	      int errno_save = errno;
	      g_free (fn);
	      errno = errno_save;
	      return -1;
	    }
	}
      else if (!g_file_test (fn, G_FILE_TEST_IS_DIR))
	{
	  g_free (fn);
	  errno = ENOTDIR;
	  return -1;
	}
      if (p)
	{
	  *p++ = G_DIR_SEPARATOR;
	  while (*p && G_IS_DIR_SEPARATOR (*p))
	    p++;
	}
    }
  while (p);

  g_free (fn);

  return 0;
}
