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

#include "asb-utils.h"
#include "asb-plugin.h"

#define ASB_METADATA_CACHE_VERSION	4

/**
 * asb_utils_get_builder_id:
 *
 * Gets the builder-id used at this time. This is unstable, and may be affected
 * by the time or by the whim of upstream.
 *
 * Returns: utf8 string
 *
 * Since: 0.2.5
 **/
gchar *
asb_utils_get_builder_id (void)
{
	return g_strdup_printf ("appstream-glib:%i",
				ASB_METADATA_CACHE_VERSION);
}

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
	return g_path_get_basename (filename);
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
 * asb_utils_ensure_exists:
 * @directory: utf8 directory name
 * @error: A #GError or %NULL
 *
 * Ensures a directory exists.
 *
 * Returns: %TRUE for success, %FALSE otherwise
 *
 * Since: 0.3.0
 **/
gboolean
asb_utils_ensure_exists (const gchar *directory, GError **error)
{
	if (g_file_test (directory, G_FILE_TEST_EXISTS))
		return TRUE;
	if (g_mkdir_with_parents (directory, 0700) != 0) {
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_FAILED,
			     "Failed to create: %s", directory);
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
	g_autoptr(GDir) dir = NULL;

	/* does directory exist */
	if (!asb_utils_ensure_exists (directory, error))
		return FALSE;

	/* try to open */
	dir = g_dir_open (directory, 0, error);
	if (dir == NULL)
		return FALSE;

	/* find each */
	while ((filename = g_dir_read_name (dir))) {
		g_autofree gchar *src = NULL;
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
 * asb_utils_count_directories_deep:
 **/
static const guint
asb_utils_count_directories_deep (const gchar *path)
{
	guint cnt = 0;
	guint i;

	for (i = 0; path[i] != '\0'; i++) {
		if (path[i] != '/')
			continue;
		cnt++;
	}
	return cnt;
}

/**
 * asb_utils_get_back_to_root:
 **/
static gchar *
asb_utils_get_back_to_root (guint levels)
{
	GString *str;
	guint i;

	str = g_string_new ("");
	for (i = 0; i < levels; i++)
		g_string_append (str, "../");
	return g_string_free (str, FALSE);
}

/**
 * asb_utils_sanitise_path:
 *
 * Converts various formats into an absolute path.
 **/
static gchar *
asb_utils_sanitise_path (const gchar *path)
{
	/* /usr/share/README -> /usr/share/README */
	if (path[0] == '/')
		return g_strdup (path);

	/* ./usr/share/README -> /usr/share/README */
	if (path[0] == '.')
		return g_strdup (path + 1);

	/* usr/share/README -> /usr/share/README */
	return g_strconcat ("/", path, NULL);
}

/**
 * asb_utils_explode_file:
 **/
static gboolean
asb_utils_explode_file (struct archive_entry *entry, const gchar *dir)
{
	const gchar *tmp;
	guint symlink_depth;
	g_autofree gchar *back_up = NULL;
	g_autofree gchar *path = NULL;
	g_autofree gchar *buf = NULL;

	/* no output file */
	if (archive_entry_pathname (entry) == NULL)
		return FALSE;

	/* update output path */
	tmp = archive_entry_pathname (entry);
	path = asb_utils_sanitise_path (tmp);
	buf = g_build_filename (dir, path, NULL);
	archive_entry_update_pathname_utf8 (entry, buf);

	/* update hardlinks */
	tmp = archive_entry_hardlink (entry);
	if (tmp != NULL) {
		g_autofree gchar *buf_link = NULL;
		g_autofree gchar *path_link = NULL;
		path_link = asb_utils_sanitise_path (tmp);
		buf_link = g_build_filename (dir, path_link, NULL);
		if (!g_file_test (buf_link, G_FILE_TEST_EXISTS)) {
			g_warning ("%s does not exist, cannot hardlink", tmp);
			return FALSE;
		}
		archive_entry_update_hardlink_utf8 (entry, buf_link);
	}

	/* update symlinks */
	tmp = archive_entry_symlink (entry);
	if (tmp != NULL) {
		g_autofree gchar *buf_link = NULL;
		symlink_depth = asb_utils_count_directories_deep (path) - 1;
		back_up = asb_utils_get_back_to_root (symlink_depth);
		if (tmp[0] == '/')
			tmp++;
		buf_link = g_build_filename (back_up, tmp, NULL);
		archive_entry_update_symlink_utf8 (entry, buf_link);
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
	const gchar *tmp;
	gboolean ret = TRUE;
	gboolean valid;
	int r;
	struct archive *arch = NULL;
	struct archive *arch_preview = NULL;
	struct archive_entry *entry;
	g_autoptr(GHashTable) matches = NULL;

	/* populate a hash with all the files, symlinks and hardlinks that
	 * actually need decompressing */
	arch_preview = archive_read_new ();
	archive_read_support_format_all (arch_preview);
	archive_read_support_filter_all (arch_preview);
	r = archive_read_open_filename (arch_preview, filename, 1024 * 32);
	if (r) {
		ret = FALSE;
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_FAILED,
			     "Cannot open: %s",
			     archive_error_string (arch_preview));
		goto out;
	}
	matches = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	for (;;) {
		g_autofree gchar *path = NULL;
		r = archive_read_next_header (arch_preview, &entry);
		if (r == ARCHIVE_EOF)
			break;
		if (r != ARCHIVE_OK) {
			ret = FALSE;
			g_set_error (error,
				     ASB_PLUGIN_ERROR,
				     ASB_PLUGIN_ERROR_FAILED,
				     "Cannot read header: %s",
				     archive_error_string (arch_preview));
			goto out;
		}

		/* get the destination filename */
		tmp = archive_entry_pathname (entry);
		if (tmp == NULL)
			continue;
		path = asb_utils_sanitise_path (tmp);
		if (glob != NULL) {
			if (asb_glob_value_search (glob, path) == NULL)
				continue;
		}
		g_hash_table_insert (matches, g_strdup (path), GINT_TO_POINTER (1));

		/* add hardlink */
		tmp = archive_entry_hardlink (entry);
		if (tmp != NULL) {
			g_hash_table_insert (matches,
					     asb_utils_sanitise_path (tmp),
					     GINT_TO_POINTER (1));
		}

		/* add symlink */
		tmp = archive_entry_symlink (entry);
		if (tmp != NULL) {
			g_hash_table_insert (matches,
					     asb_utils_sanitise_path (tmp),
					     GINT_TO_POINTER (1));
		}
	}

	/* decompress anything matching either glob */
	arch = archive_read_new ();
	archive_read_support_format_all (arch);
	archive_read_support_filter_all (arch);
	r = archive_read_open_filename (arch, filename, 1024 * 32);
	if (r) {
		ret = FALSE;
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_FAILED,
			     "Cannot open: %s",
			     archive_error_string (arch));
		goto out;
	}
	for (;;) {
		g_autofree gchar *path = NULL;
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
		tmp = archive_entry_pathname (entry);
		path = asb_utils_sanitise_path (tmp);
		if (g_hash_table_lookup (matches, path) == NULL)
			continue;
		valid = asb_utils_explode_file (entry, dir);
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
	if (arch_preview != NULL) {
		archive_read_close (arch_preview);
		archive_read_free (arch_preview);
	}
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
			 const gchar *path_orig,
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
	if (g_str_has_suffix (filename, ".gz"))
		archive_write_add_filter_gzip (a);
	if (g_str_has_suffix (filename, ".bz2"))
		archive_write_add_filter_bzip2 (a);
	if (g_str_has_suffix (filename, ".xz"))
		archive_write_add_filter_xz (a);
	archive_write_set_format_pax_restricted (a);
	archive_write_open_filename (a, filename);
	for (i = 0; i < files->len; i++) {
		g_autofree gchar *data = NULL;
		g_autofree gchar *filename_full = NULL;

		tmp = g_ptr_array_index (files, i);
		filename_full = g_build_filename (path_orig, tmp, NULL);
		stat (filename_full, &st);
		entry = archive_entry_new ();
		archive_entry_set_pathname (entry, tmp);
		archive_entry_set_size (entry, st.st_size);
		archive_entry_set_filetype (entry, AE_IFREG);
		archive_entry_set_perm (entry, 0644);
		archive_write_header (a, entry);
		ret = g_file_get_contents (filename_full, &data, &len, error);
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
 * asb_utils_add_files_recursive:
 **/
static gboolean
asb_utils_add_files_recursive (GPtrArray *files,
			       const gchar *path_orig,
			       const gchar *path,
			       GError **error)
{
	const gchar *path_trailing;
	const gchar *tmp;
	guint path_orig_len;
	g_autoptr(GDir) dir = NULL;

	dir = g_dir_open (path, 0, error);
	if (dir == NULL)
		return FALSE;
	path_orig_len = strlen (path_orig);
	while ((tmp = g_dir_read_name (dir)) != NULL) {
		g_autofree gchar *path_new = NULL;
		path_new = g_build_filename (path, tmp, NULL);
		if (g_file_test (path_new, G_FILE_TEST_IS_DIR)) {
			if (!asb_utils_add_files_recursive (files, path_orig, path_new, error))
				return FALSE;
		} else {
			path_trailing = path_new + path_orig_len + 1;
			g_ptr_array_add (files, g_strdup (path_trailing));
		}
	}
	return TRUE;
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
	g_autoptr(GPtrArray) files = NULL;

	/* add all files in the directory to the archive */
	files = g_ptr_array_new_with_free_func (g_free);
	if (!asb_utils_add_files_recursive (files, directory, directory, error))
		return FALSE;
	if (files->len == 0)
		return TRUE;

	/* sort by filename for deterministic results */
	g_ptr_array_sort (files, g_strcmp0);

	/* write tar file */
	return asb_utils_write_archive (filename, directory, files, error);
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

	g_return_val_if_fail (array != NULL, NULL);
	g_return_val_if_fail (search != NULL, NULL);

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
