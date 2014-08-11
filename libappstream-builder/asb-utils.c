/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * SECTION:asb-utils
 * @short_description: Helper functionality.
 * @stability: Unstable
 */

#include "config.h"

#include <glib/gstdio.h>
#include <fnmatch.h>
#include <archive.h>
#include <archive_entry.h>
#include <string.h>

#include "as-cleanup.h"
#include "asb-utils.h"
#include "asb-plugin.h"

#define ASB_METADATA_CACHE_VERSION	1

/**
 * asb_utils_get_cache_id_for_filename:
 * @filename: utf8 filename
 *
 * Gets the cache-id for a given filename.
 *
 * Returns: utf8 string
 *
 * Since: 0.1.0
 **/
gchar *
asb_utils_get_cache_id_for_filename (const gchar *filename)
{
	_cleanup_free_ gchar *basename;

	/* only use the basename and the cache version */
	basename = g_path_get_basename (filename);
	return g_strdup_printf ("%s:%i",
				basename,
				ASB_METADATA_CACHE_VERSION);
}

/**
 * asb_utils_rmtree:
 * @directory: utf8 directory name
 * @error: A #GError or %NULL
 *
 * Removes a directory tree.
 *
 * Returns: %TRUE for success, %FALSE otherwise
 *
 * Since: 0.1.0
 **/
gboolean
asb_utils_rmtree (const gchar *directory, GError **error)
{
	gint rc;
	gboolean ret;

	ret = asb_utils_ensure_exists_and_empty (directory, error);
	if (!ret)
		return FALSE;
	rc = g_remove (directory);
	if (rc != 0) {
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_FAILED,
			     "Failed to delete: %s", directory);
		return FALSE;
	}
	return TRUE;
}

/**
 * asb_utils_ensure_exists_and_empty:
 * @directory: utf8 directory name
 * @error: A #GError or %NULL
 *
 * Ensures a directory exists and empty.
 *
 * Returns: %TRUE for success, %FALSE otherwise
 *
 * Since: 0.1.0
 **/
gboolean
asb_utils_ensure_exists_and_empty (const gchar *directory, GError **error)
{
	const gchar *filename;
	_cleanup_dir_close_ GDir *dir = NULL;

	/* does directory exist */
	if (!g_file_test (directory, G_FILE_TEST_EXISTS)) {
		if (g_mkdir_with_parents (directory, 0700) != 0) {
			g_set_error (error,
				     ASB_PLUGIN_ERROR,
				     ASB_PLUGIN_ERROR_FAILED,
				     "Failed to create: %s", directory);
			return FALSE;
		}
		return TRUE;
	}

	/* try to open */
	dir = g_dir_open (directory, 0, error);
	if (dir == NULL)
		return FALSE;

	/* find each */
	while ((filename = g_dir_read_name (dir))) {
		_cleanup_free_ gchar *src;
		src = g_build_filename (directory, filename, NULL);
		if (g_file_test (src, G_FILE_TEST_IS_DIR)) {
			if (!asb_utils_rmtree (src, error))
				return FALSE;
		} else {
			if (g_unlink (src) != 0) {
				g_set_error (error,
					     ASB_PLUGIN_ERROR,
					     ASB_PLUGIN_ERROR_FAILED,
					     "Failed to delete: %s", src);
				return FALSE;
			}
		}
	}
	return TRUE;
}

/**
 * asb_utils_explode_file:
 **/
static gboolean
asb_utils_explode_file (struct archive_entry *entry,
			const gchar *dir,
			GPtrArray *glob)
{
	const gchar *tmp;
	gchar buf[PATH_MAX];
	_cleanup_free_ gchar *path = NULL;

	/* no output file */
	if (archive_entry_pathname (entry) == NULL)
		return FALSE;

	/* do we have to decompress this file */
	tmp = archive_entry_pathname (entry);
	if (glob != NULL) {
		if (tmp[0] == '/') {
			path = g_strdup (tmp);
		} else if (tmp[0] == '.') {
			path = g_strdup (tmp + 1);
		} else {
			path = g_strconcat ("/", tmp, NULL);
		}
		if (asb_glob_value_search (glob, path) == NULL)
			return FALSE;
	}

	/* update output path */
	g_snprintf (buf, PATH_MAX, "%s/%s", dir, tmp);
	archive_entry_update_pathname_utf8 (entry, buf);

	/* update hardlinks */
	tmp = archive_entry_hardlink (entry);
	if (tmp != NULL) {
		g_snprintf (buf, PATH_MAX, "%s/%s", dir, tmp);
		archive_entry_update_hardlink_utf8 (entry, buf);
	}

	/* update symlinks */
	tmp = archive_entry_symlink (entry);
	if (tmp != NULL) {
		g_snprintf (buf, PATH_MAX, "%s/%s", dir, tmp);
		archive_entry_update_symlink_utf8 (entry, buf);
	}
	return TRUE;
}

/**
 * asb_utils_explode:
 * @filename: package filename
 * @dir: directory to decompress into
 * @glob: (element-type utf8): filename globs, or %NULL
 * @error: A #GError or %NULL
 *
 * Decompresses the package into a given directory.
 *
 * Returns: %TRUE for success, %FALSE otherwise
 *
 * Since: 0.1.0
 **/
gboolean
asb_utils_explode (const gchar *filename,
		   const gchar *dir,
		   GPtrArray *glob,
		   GError **error)
{
	gboolean ret = TRUE;
	gboolean valid;
	gsize len;
	int r;
	struct archive *arch = NULL;
	struct archive_entry *entry;
	_cleanup_free_ gchar *data = NULL;

	/* load file at once to avoid seeking */
	ret = g_file_get_contents (filename, &data, &len, error);
	if (!ret)
		goto out;

	/* read anything */
	arch = archive_read_new ();
	archive_read_support_format_all (arch);
	archive_read_support_filter_all (arch);
	r = archive_read_open_memory (arch, data, len);
	if (r) {
		ret = FALSE;
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_FAILED,
			     "Cannot open: %s",
			     archive_error_string (arch));
		goto out;
	}

	/* decompress each file */
	for (;;) {
		r = archive_read_next_header (arch, &entry);
		if (r == ARCHIVE_EOF)
			break;
		if (r != ARCHIVE_OK) {
			ret = FALSE;
			g_set_error (error,
				     ASB_PLUGIN_ERROR,
				     ASB_PLUGIN_ERROR_FAILED,
				     "Cannot read header: %s",
				     archive_error_string (arch));
			goto out;
		}

		/* only extract if valid */
		valid = asb_utils_explode_file (entry, dir, glob);
		if (!valid)
			continue;
		r = archive_read_extract (arch, entry, 0);
		if (r != ARCHIVE_OK) {
			ret = FALSE;
			g_set_error (error,
				     ASB_PLUGIN_ERROR,
				     ASB_PLUGIN_ERROR_FAILED,
				     "Cannot extract: %s",
				     archive_error_string (arch));
			goto out;
		}
	}
out:
	if (arch != NULL) {
		archive_read_close (arch);
		archive_read_free (arch);
	}
	return ret;
}

/**
 * asb_utils_write_archive:
 **/
static gboolean
asb_utils_write_archive (const gchar *filename,
			 GPtrArray *files,
			 GError **error)
{
	const gchar *tmp;
	gboolean ret = TRUE;
	gsize len;
	guint i;
	struct archive *a;
	struct archive_entry *entry;
	struct stat st;

	a = archive_write_new ();
	archive_write_add_filter_gzip (a);
	archive_write_set_format_pax_restricted (a);
	archive_write_open_filename (a, filename);
	for (i = 0; i < files->len; i++) {
		_cleanup_free_ gchar *basename;
		_cleanup_free_ gchar *data = NULL;
		tmp = g_ptr_array_index (files, i);
		stat (tmp, &st);
		entry = archive_entry_new ();
		basename = g_path_get_basename (tmp);
		archive_entry_set_pathname (entry, basename);
		archive_entry_set_size (entry, st.st_size);
		archive_entry_set_filetype (entry, AE_IFREG);
		archive_entry_set_perm (entry, 0644);
		archive_write_header (a, entry);
		ret = g_file_get_contents (tmp, &data, &len, error);
		if (!ret)
			goto out;
		archive_write_data (a, data, len);
		archive_entry_free (entry);
	}
out:
	archive_write_close (a);
	archive_write_free (a);
	return ret;
}

/**
 * asb_utils_write_archive_dir:
 * @filename: archive filename
 * @directory: source directory
 * @error: A #GError or %NULL
 *
 * Writes an archive from a directory.
 *
 * Returns: %TRUE for success, %FALSE otherwise
 *
 * Since: 0.1.0
 **/
gboolean
asb_utils_write_archive_dir (const gchar *filename,
			     const gchar *directory,
			     GError **error)
{
	GPtrArray *files = NULL;
	GDir *dir;
	const gchar *tmp;
	gboolean ret = TRUE;

	/* add all files in the directory to the archive */
	dir = g_dir_open (directory, 0, error);
	if (dir == NULL) {
		ret = FALSE;
		goto out;
	}
	files = g_ptr_array_new_with_free_func (g_free);
	while ((tmp = g_dir_read_name (dir)) != NULL)
		g_ptr_array_add (files, g_build_filename (directory, tmp, NULL));

	/* write tar file */
	ret = asb_utils_write_archive (filename, files, error);
	if (!ret)
		goto out;
out:
	if (dir != NULL)
		g_dir_close (dir);
	if (files != NULL)
		g_ptr_array_unref (files);
	return ret;
}

/**
 * asb_utils_add_apps_from_file:
 * @apps: (element-type AsbApp): applications
 * @filename: XML file to load
 * @error: A #GError or %NULL
 *
 * Add applications from a file.
 *
 * Returns: %TRUE for success, %FALSE otherwise
 *
 * Since: 0.1.0
 **/
gboolean
asb_utils_add_apps_from_file (GList **apps, const gchar *filename, GError **error)
{
	AsApp *app;
	AsStore *store;
	GFile *file;
	GPtrArray *array;
	gboolean ret;
	guint i;

	/* parse file */
	store = as_store_new ();
	file = g_file_new_for_path (filename);
	ret = as_store_from_file (store, file, NULL, NULL, error);
	if (!ret)
		goto out;

	/* copy Asapp's into AsbApp's */
	array = as_store_get_apps (store);
	for (i = 0; i < array->len; i++) {
		app = g_ptr_array_index (array, i);
		asb_plugin_add_app (apps, app);
	}
out:
	g_object_unref (file);
	g_object_unref (store);
	return ret;
}

/**
 * asb_utils_add_apps_from_dir:
 * @apps: (element-type AsbApp): applications
 * @path: path to read
 * @error: A #GError or %NULL
 *
 * Add applications from a directory.
 *
 * Returns: %TRUE for success, %FALSE otherwise
 *
 * Since: 0.1.0
 **/
gboolean
asb_utils_add_apps_from_dir (GList **apps, const gchar *path, GError **error)
{
	const gchar *tmp;
	gboolean ret = TRUE;
	gchar *filename;
	GDir *dir;

	dir = g_dir_open (path, 0, error);
	if (dir == NULL) {
		ret = FALSE;
		goto out;
	}
	while ((tmp = g_dir_read_name (dir)) != NULL) {
		filename = g_build_filename (path, tmp, NULL);
		ret = asb_utils_add_apps_from_file (apps, filename, error);
		g_free (filename);
		if (!ret)
			goto out;
	}
out:
	if (dir != NULL)
		g_dir_close (dir);
	return ret;
}

/**
 * asb_string_replace:
 * @string: Source string
 * @search: utf8 string to search for
 * @replace: utf8 string to replace with
 *
 * Does search/replace on a given string.
 *
 * Returns: the number of times the string was replaced
 *
 * Since: 0.1.0
 **/
guint
asb_string_replace (GString *string, const gchar *search, const gchar *replace)
{
	gchar **split = NULL;
	gchar *tmp = NULL;
	guint count = 0;

	/* quick search */
	if (g_strstr_len (string->str, -1, search) == NULL)
		goto out;

	/* replace */
	split = g_strsplit (string->str, search, -1);
	tmp = g_strjoinv (replace, split);
	g_string_assign (string, tmp);
	count = g_strv_length (split) - 1;
out:
	g_strfreev (split);
	g_free (tmp);
	return count;
}

/******************************************************************************/

struct AsbGlobValue {
	gchar		*glob;
	gchar		*value;
};

/**
 * asb_glob_value_free: (skip)
 * @kv: key-value
 *
 * Frees a #AsbGlobValue.
 *
 * Since: 0.1.0
 **/
void
asb_glob_value_free (AsbGlobValue *kv)
{
	g_free (kv->glob);
	g_free (kv->value);
	g_slice_free (AsbGlobValue, kv);
}

/**
 * asb_glob_value_array_new: (skip)
 *
 * Creates a new value array.
 *
 * Returns: (element-type AsbGlobValue): A new array
 *
 * Since: 0.1.0
 **/
GPtrArray *
asb_glob_value_array_new (void)
{
	return g_ptr_array_new_with_free_func ((GDestroyNotify) asb_glob_value_free);
}

/**
 * asb_glob_value_new: (skip)
 * @glob: utf8 string
 * @value: utf8 string
 *
 * Creates a new value.
 *
 * Returns: a #AsbGlobValue
 *
 * Since: 0.1.0
 **/
AsbGlobValue *
asb_glob_value_new (const gchar *glob, const gchar *value)
{
	AsbGlobValue *kv;
	kv = g_slice_new0 (AsbGlobValue);
	kv->glob = g_strdup (glob);
	kv->value = g_strdup (value);
	return kv;
}

/**
 * asb_glob_value_search: (skip)
 * @array: of AsbGlobValue, keys may contain globs
 * @search: may not be a glob
 *
 * Searches for a glob value.
 *
 * Returns: value, or %NULL
 *
 * Since: 0.1.0
 */
const gchar *
asb_glob_value_search (GPtrArray *array, const gchar *search)
{
	const AsbGlobValue *tmp;
	guint i;

	/* invalid */
	if (search == NULL)
		return NULL;

	for (i = 0; i < array->len; i++) {
		tmp = g_ptr_array_index (array, i);
		if (fnmatch (tmp->glob, search, 0) == 0)
			return tmp->value;
	}
	return NULL;
}
