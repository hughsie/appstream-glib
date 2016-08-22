/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Richard Hughes <richard@hughsie.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "config.h"

#include <libgcab.h>
#include <glib/gstdio.h>
#include <gio/gunixinputstream.h>

#include "as-store-cab.h"
#include "as-utils.h"

#ifndef GCabCabinet_autoptr
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GCabCabinet, g_object_unref)
#endif

static gboolean
as_store_cab_cb (GCabFile *file, gpointer user_data)
{
	GPtrArray *filelist = (GPtrArray *) user_data;
	const gchar *fn;
	gchar *tmp;

	/* strip Windows or Linux paths */
	fn = gcab_file_get_name (file);
	tmp = g_strrstr (fn, "\\");
	if (tmp == NULL)
		tmp = g_strrstr (fn, "/");
	if (tmp != NULL) {
		g_debug ("removed path prefix for %s", fn);
		fn = tmp + 1;
	}
	gcab_file_set_extract_name (file, fn);

	g_ptr_array_add (filelist, g_strdup (fn));
	return TRUE;
}

static gboolean
as_store_cab_verify_checksum_cab (AsRelease *release,
				  GBytes *bytes,
				  GError **error)
{
	AsChecksum *csum_tmp;
	g_autofree gchar *actual = NULL;

	/* nothing already set, so just add */
	actual = g_compute_checksum_for_bytes (G_CHECKSUM_SHA1, bytes);
	csum_tmp = as_release_get_checksum_by_target (release, AS_CHECKSUM_TARGET_CONTAINER);
	if (as_checksum_get_value (csum_tmp) == NULL) {
		as_checksum_set_kind (csum_tmp, G_CHECKSUM_SHA1);
		as_checksum_set_value (csum_tmp, actual);
		return TRUE;
	}

	/* check it matches */
	if (g_strcmp0 (actual, as_checksum_get_value (csum_tmp)) != 0) {
		g_set_error (error,
			     AS_STORE_ERROR,
			     AS_STORE_ERROR_FAILED,
			     "container checksum invalid, expected %s, got %s",
			     as_checksum_get_value (csum_tmp), actual);
		return FALSE;
	}

	/* success */
	return TRUE;
}

static gboolean
as_store_cab_verify_checksum_fw (AsChecksum *checksum,
				 const gchar *tmp_path,
				 GError **error)
{
	g_autofree gchar *rel_basename = NULL;
	g_autofree gchar *rel_fn = NULL;
	g_autofree gchar *actual = NULL;

	/* get the firmware filename */
	rel_basename = g_path_get_basename (as_checksum_get_filename (checksum));
	rel_fn = g_build_filename (tmp_path, rel_basename, NULL);

	/* calculate checksum */
	if (g_file_test (rel_fn, G_FILE_TEST_EXISTS)) {
		gsize len;
		g_autofree gchar *data = NULL;
		if (!g_file_get_contents (rel_fn, &data, &len, error))
			return FALSE;
		actual = g_compute_checksum_for_data (G_CHECKSUM_SHA1, (guchar *)data, len);
	}

	/* nothing already set, so just add */
	if (as_checksum_get_value (checksum) == NULL) {
		as_checksum_set_kind (checksum, G_CHECKSUM_SHA1);
		as_checksum_set_value (checksum, actual);
		return TRUE;
	}

	/* check it matches */
	if (g_strcmp0 (actual, as_checksum_get_value (checksum)) != 0) {
		g_set_error (error,
			     AS_STORE_ERROR,
			     AS_STORE_ERROR_FAILED,
			     "contents checksum invalid, expected %s, got %s",
			     as_checksum_get_value (checksum), actual);
		return FALSE;
	}

	/* success */
	return TRUE;
}

static gboolean
as_store_cab_set_release_blobs (AsRelease *release, const gchar *tmp_path, GError **error)
{
	AsChecksum *csum_tmp;
	g_autofree gchar *asc_basename = NULL;
	g_autofree gchar *asc_fn = NULL;
	g_autofree gchar *rel_basename = NULL;
	g_autofree gchar *rel_fn = NULL;
	g_autoptr(GError) error_local = NULL;

	/* get the firmware filename */
	csum_tmp = as_release_get_checksum_by_target (release, AS_CHECKSUM_TARGET_CONTENT);
	g_assert (csum_tmp != NULL);
	rel_basename = g_path_get_basename (as_checksum_get_filename (csum_tmp));
	rel_fn = g_build_filename (tmp_path, rel_basename, NULL);

	/* add this information to the release objects */
	if (g_file_test (rel_fn, G_FILE_TEST_EXISTS)) {
		gchar *data = NULL;
		gsize data_len = 0;
		g_autoptr(GBytes) blob = NULL;

		if (!g_file_get_contents (rel_fn, &data, &data_len, &error_local)) {
			g_set_error (error,
				     AS_STORE_ERROR,
				     AS_STORE_ERROR_FAILED,
				     "failed to open %s: %s",
				     rel_fn, error_local->message);
			return FALSE;
		}

		/* this is the size of the firmware */
		if (as_release_get_size (release, AS_SIZE_KIND_INSTALLED) == 0) {
			as_release_set_size (release,
					     AS_SIZE_KIND_INSTALLED,
					     data_len);
		}

		/* set the data on the release object */
		blob = g_bytes_new_take (data, data_len);
		as_release_set_blob (release, rel_basename, blob);
	}

	/* if the signing file exists, set that too */
	asc_basename = g_strdup_printf ("%s.asc", rel_basename);
	asc_fn = g_build_filename (tmp_path, asc_basename, NULL);
	if (g_file_test (asc_fn, G_FILE_TEST_EXISTS)) {
		gchar *data = NULL;
		gsize data_len = 0;
		g_autoptr(GBytes) blob = NULL;

		if (!g_file_get_contents (asc_fn, &data, &data_len, &error_local)) {
			g_set_error (error,
				     AS_STORE_ERROR,
				     AS_STORE_ERROR_FAILED,
				     "failed to open %s: %s",
				     asc_fn, error_local->message);
			return FALSE;
		}

		/* set the data on the release object */
		blob = g_bytes_new_take (data, data_len);
		as_release_set_blob (release, asc_basename, blob);
	}
	return TRUE;
}

static gboolean
as_store_cab_from_bytes_with_origin (AsStore *store,
				     GBytes *bytes,
				     const gchar *basename,
				     GCancellable *cancellable,
				     GError **error)
{
	g_autoptr(GCabCabinet) gcab = NULL;
	g_autoptr(GError) error_local = NULL;
	g_autofree gchar *tmp_path = NULL;
	g_autoptr(GFile) tmp_file = NULL;
	g_autoptr(GInputStream) input_stream = NULL;
	g_autoptr(GPtrArray) filelist = NULL;
	guint i;

	/* open the file */
	gcab = gcab_cabinet_new ();
	input_stream = g_memory_input_stream_new_from_bytes (bytes);
	if (!gcab_cabinet_load (gcab, input_stream, NULL, &error_local)) {
		g_set_error (error,
			     AS_STORE_ERROR,
			     AS_STORE_ERROR_FAILED,
			     "cannot load .cab file: %s",
			     error_local->message);
		return FALSE;
	}

	/* decompress to /tmp */
	tmp_path = g_dir_make_tmp ("appstream-glib-XXXXXX", &error_local);
	if (tmp_path == NULL) {
		g_set_error (error,
			     AS_STORE_ERROR,
			     AS_STORE_ERROR_FAILED,
			     "failed to create temp dir: %s",
			     error_local->message);
		return FALSE;
	}

	/* extract the entire cab file */
	filelist = g_ptr_array_new_with_free_func (g_free);
	tmp_file = g_file_new_for_path (tmp_path);
	if (!gcab_cabinet_extract_simple (gcab, tmp_file,
					  as_store_cab_cb,
					  filelist, NULL, &error_local)) {
		g_set_error (error,
			     AS_STORE_ERROR,
			     AS_STORE_ERROR_FAILED,
			     "failed to extract .cab file: %s",
			     error_local->message);
		return FALSE;
	}

	/* loop through each file looking for components */
	for (i = 0; i < filelist->len; i++) {
		AsRelease *rel;
		AsChecksum *csum_tmp;
		const gchar *fn;
		g_autofree gchar *tmp_fn = NULL;
		g_autoptr(AsApp) app = NULL;

		/* debug */
		fn = g_ptr_array_index (filelist, i);
		g_debug ("found file %u\t%s", i, fn);

		/* if inf or metainfo, add */
		if (as_app_guess_source_kind (fn) != AS_APP_SOURCE_KIND_METAINFO)
			continue;

		tmp_fn = g_build_filename (tmp_path, fn, NULL);
		app = as_app_new ();
		if (!as_app_parse_file (app, tmp_fn,
					AS_APP_PARSE_FLAG_NONE, &error_local)) {
			g_set_error (error,
				     AS_STORE_ERROR,
				     AS_STORE_ERROR_FAILED,
				     "%s could not be loaded: %s",
				     tmp_fn,
				     error_local->message);
			return FALSE;
		}

		/* only process the latest release */
		rel = as_app_get_release_default (app);
		if (rel == NULL) {
			g_set_error_literal (error,
					     AS_STORE_ERROR,
					     AS_STORE_ERROR_FAILED,
					     "no releases in metainfo file");
			return FALSE;
		}

		/* ensure we always have a container checksum */
		csum_tmp = as_release_get_checksum_by_target (rel, AS_CHECKSUM_TARGET_CONTAINER);
		if (csum_tmp == NULL) {
			g_autoptr(AsChecksum) csum = NULL;
			csum = as_checksum_new ();
			as_checksum_set_target (csum, AS_CHECKSUM_TARGET_CONTAINER);
			if (basename != NULL)
				as_checksum_set_filename (csum, basename);
			as_release_add_checksum (rel, csum);
		}

		/* set the container checksum */
		if (!as_store_cab_verify_checksum_cab (rel, bytes, error))
			return FALSE;

		/* this is the size of the cab file itself */
		if (as_release_get_size (rel, AS_SIZE_KIND_DOWNLOAD) == 0)
			as_release_set_size (rel, AS_SIZE_KIND_DOWNLOAD,
					     g_bytes_get_size (bytes));

		/* ensure we always have a content checksum */
		csum_tmp = as_release_get_checksum_by_target (rel, AS_CHECKSUM_TARGET_CONTENT);
		if (csum_tmp == NULL) {
			g_autoptr(AsChecksum) csum = NULL;
			csum = as_checksum_new ();
			as_checksum_set_target (csum, AS_CHECKSUM_TARGET_CONTENT);
			/* if this isn't true, a firmware needs to set in
			 * the metainfo.xml file something like:
			 * <checksum target="content" filename="FLASH.ROM"/> */
			as_checksum_set_filename (csum, "firmware.bin");
			as_release_add_checksum (rel, csum);
			csum_tmp = csum;
		}
		if (!as_store_cab_verify_checksum_fw (csum_tmp, tmp_path, error))
			return FALSE;

		/* set blobs */
		if (!as_store_cab_set_release_blobs (rel, tmp_path, error))
			return FALSE;

		/* add any component to the store */
		as_store_add_app (store, app);
	}

	/* if we provided an .inf file check the values matched */
	for (i = 0; i < filelist->len; i++) {
		AsApp *app_tmp;
		AsChecksum *csum_inf;
		AsChecksum *csum_metainfo;
		AsRelease *rel_inf;
		AsRelease *rel_metainfo;
		const gchar *fn;
		g_autoptr(AsApp) app_inf = NULL;
		g_autofree gchar *tmp_fn = NULL;

		fn = g_ptr_array_index (filelist, i);
		if (as_app_guess_source_kind (fn) != AS_APP_SOURCE_KIND_INF)
			continue;

		/* get the id from the store */
		app_inf = as_app_new ();
		tmp_fn = g_build_filename (tmp_path, fn, NULL);
		if (!as_app_parse_file (app_inf, tmp_fn, AS_APP_PARSE_FLAG_NONE, error))
			return FALSE;
		app_tmp = as_store_get_app_by_provide (store,
						       AS_PROVIDE_KIND_FIRMWARE_FLASHED,
						       as_app_get_id (app_inf));
		if (app_tmp == NULL) {
			g_set_error (error,
				     AS_STORE_ERROR,
				     AS_STORE_ERROR_FAILED,
				     "no metainfo file with provide %s",
				     as_app_get_id (app_inf));
			return FALSE;
		}

		/* check the version matches */
		rel_inf = as_app_get_release_default (app_inf);
		rel_metainfo = as_app_get_release_default (app_tmp);
		if (as_release_get_version (rel_inf) != NULL &&
		    as_utils_vercmp (as_release_get_version (rel_inf),
				     as_release_get_version (rel_metainfo)) != 0) {
			g_set_error (error,
				     AS_STORE_ERROR,
				     AS_STORE_ERROR_FAILED,
				     "metainfo version is %s while inf has %s",
				     as_release_get_version (rel_metainfo),
				     as_release_get_version (rel_inf));
			return FALSE;
		}

		/* verify Firmware_CopyFiles matches */
		csum_inf = as_release_get_checksum_by_target (rel_inf, AS_CHECKSUM_TARGET_CONTENT);
		csum_metainfo = as_release_get_checksum_by_target (rel_metainfo, AS_CHECKSUM_TARGET_CONTENT);
		if (g_strcmp0 (as_checksum_get_filename (csum_inf),
			       as_checksum_get_filename (csum_metainfo)) != 0) {
			g_set_error (error,
				     AS_STORE_ERROR,
				     AS_STORE_ERROR_FAILED,
				     "metainfo filename is %s while inf has %s",
				     as_checksum_get_filename (csum_metainfo),
				     as_checksum_get_filename (csum_inf));
			return FALSE;
		}
	}

	/* delete temp files */
	for (i = 0; i < filelist->len; i++) {
		const gchar *fn;
		g_autofree gchar *tmp_fn = NULL;
		fn = g_ptr_array_index (filelist, i);
		tmp_fn = g_build_filename (tmp_path, fn, NULL);
		g_unlink (tmp_fn);
	}
	g_rmdir (tmp_path);

	/* success */
	return TRUE;
}

gboolean
as_store_cab_from_bytes (AsStore *store,
			 GBytes *bytes,
			 GCancellable *cancellable,
			 GError **error)
{
	return as_store_cab_from_bytes_with_origin (store, bytes, NULL,
						    cancellable, error);
}

gboolean
as_store_cab_from_file (AsStore *store,
			GFile *file,
			GCancellable *cancellable,
			GError **error)
{
	guint64 size;
	g_autoptr(GBytes) bytes = NULL;
	g_autoptr(GError) error_local = NULL;
	g_autoptr(GFileInfo) info = NULL;
	g_autoptr(GInputStream) input_stream = NULL;
	g_autofree gchar *filename = NULL;
	g_autofree gchar *origin = NULL;

	/* set origin */
	origin = g_file_get_basename (file);
	as_store_set_origin (store, origin);

	/* open file */
	input_stream = G_INPUT_STREAM (g_file_read (file, cancellable, &error_local));
	if (input_stream == NULL) {
		filename = g_file_get_path (file);
		g_set_error (error,
			     AS_STORE_ERROR,
			     AS_STORE_ERROR_FAILED,
			     "Failed to open %s: %s",
			     filename, error_local->message);
		return FALSE;
	}

	/* get size */
	info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_SIZE,
				  G_FILE_QUERY_INFO_NONE, cancellable, &error_local);
	if (info == NULL) {
		filename = g_file_get_path (file);
		g_set_error (error,
			     AS_STORE_ERROR,
			     AS_STORE_ERROR_FAILED,
			     "Failed to get info from %s: %s",
			     filename, error_local->message);
		return FALSE;
	}

	/* slurp it all */
	size = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_STANDARD_SIZE);
	bytes = g_input_stream_read_bytes (input_stream, (gsize) size,
					   cancellable, &error_local);
	if (bytes == NULL) {
		filename = g_file_get_path (file);
		g_set_error (error,
			     AS_STORE_ERROR,
			     AS_STORE_ERROR_FAILED,
			     "Failed to read %s: %s",
			     filename, error_local->message);
		return FALSE;
	}

	/* parse */
	return as_store_cab_from_bytes_with_origin (store, bytes, origin,
						    cancellable, error);
}
