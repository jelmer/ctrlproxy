/* 
	ctrlproxy: A modular IRC proxy
	(c) 2002-2003 Jelmer Vernooij <jelmer@jelmer.uk>

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
#include <sys/types.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <glib/gstdio.h>


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

pid_t read_pidfile(const char *path)
{
	char *contents;
	pid_t pid;
	GError *error = NULL;
	if (!g_file_get_contents(path, &contents, NULL, &error)) {
		if (error != NULL)
			g_error_free(error);
		return -1;
	}
	pid = atol(contents);

	if (pid == 0)
		return -1;

	/* Check if process is still running */
	if (kill(pid,0) != 0 && errno == ESRCH)
		pid = -1;

	g_free(contents);
	return pid;
}

gboolean write_pidfile(const char *path)
{
	GError *error = NULL;
	char contents[100];
	snprintf(contents, 100, "%u", getpid());
	if (!g_file_set_contents(path, contents, -1, &error)) {
		log_global(LOG_ERROR, "Unable to write pid file `%s': %s", path, error->message);
		g_error_free(error);
		return FALSE;
	}
	return TRUE;
}


