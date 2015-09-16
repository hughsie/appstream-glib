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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <config.h>
#include <fnmatch.h>
#include <string.h>

#include <asb-plugin.h>

/**
 * asb_plugin_get_name:
 */
const gchar *
asb_plugin_get_name (void)
{
	return "firmware";
}

/**
 * asb_plugin_add_globs:
 */
void
asb_plugin_add_globs (AsbPlugin *plugin, GPtrArray *globs)
{
	asb_plugin_add_glob (globs, "*.inf");
	asb_plugin_add_glob (globs, "*.bin");
	asb_plugin_add_glob (globs, "*.cap");
}

/**
 * asb_plugin_firmware_get_inf_fn:
 */
static gchar *
asb_plugin_firmware_get_inf_fn (const gchar *filename)
{
	gchar *basename;
	gchar *tmp;

	basename = g_path_get_basename (filename);
	tmp = g_strrstr (basename, ".metainfo.xml");
	if (tmp != NULL)
		*tmp = '\0';
	return g_strdup_printf ("%s.inf", basename);
}

/**
 * asb_plugin_firmware_get_checksum:
 */
static gchar *
asb_plugin_firmware_get_checksum (const gchar *filename,
				  GChecksumType checksum_type,
				  GError **error)
{
	gsize len;
	g_autofree gchar *data = NULL;

	if (!g_file_get_contents (filename, &data, &len, error))
		return NULL;
	return g_compute_checksum_for_data (checksum_type, (const guchar *)data, len);
}

/**
 * asb_plugin_firmware_refine:
 */
static gboolean
asb_plugin_firmware_refine (AsbPlugin *plugin,
			    AsbPackage *pkg,
			    const gchar *filename,
			    AsbApp *app,
			    const gchar *tmpdir,
			    GError **error)
{
	AsRelease *release;
	GError *error_local = NULL;
	const gchar *fw_basename = NULL;
	g_autofree gchar *checksum = NULL;
	g_autofree gchar *location_checksum = NULL;
	g_autofree gchar *metainfo_fn = NULL;
	g_autoptr(AsApp) inf = NULL;
	g_autoptr(AsChecksum) csum = NULL;

	/* parse */
	inf = as_app_new ();
	if (!as_app_parse_file (inf, filename,
				AS_APP_PARSE_FLAG_NONE, &error_local)) {
		g_set_error_literal (error,
				     ASB_PLUGIN_ERROR,
				     ASB_PLUGIN_ERROR_NOT_SUPPORTED,
				     error_local->message);
		return FALSE;
	}

	/* get the correct release, creating if required */
	release = as_app_get_release_default (AS_APP (app));
	if (release == NULL) {
		release = as_release_new ();
		as_app_add_release (AS_APP (app), release);
	}

	/* add the checksum for the .cab file */
	checksum = asb_plugin_firmware_get_checksum (asb_package_get_filename (pkg),
						     G_CHECKSUM_SHA1,
						     error);
	if (checksum == NULL)
		return FALSE;

	/* add checksum */
	csum = as_checksum_new ();
	as_checksum_set_kind (csum, G_CHECKSUM_SHA1);
	as_checksum_set_target (csum, AS_CHECKSUM_TARGET_CONTAINER);
	as_checksum_set_value (csum, checksum);
	as_checksum_set_filename (csum, asb_package_get_basename (pkg));
	as_release_add_checksum (release, csum);

	/* set the internal checksum */
	fw_basename = as_app_get_metadata_item (inf, "FirmwareBasename");
	if (fw_basename != NULL) {
		g_autofree gchar *checksum_bin = NULL;
		g_autofree gchar *fn_bin = NULL;
		g_autoptr(AsChecksum) csum_bin = NULL;

		/* add the checksum for the .bin file */
		fn_bin = g_build_filename (tmpdir, fw_basename, NULL);
		checksum_bin = asb_plugin_firmware_get_checksum (fn_bin,
								 G_CHECKSUM_SHA1,
								 error);
		if (checksum_bin == NULL)
			return FALSE;

		/* add internal checksum */
		csum_bin = as_checksum_new ();
		as_checksum_set_kind (csum_bin, G_CHECKSUM_SHA1);
		as_checksum_set_target (csum_bin, AS_CHECKSUM_TARGET_CONTENT);
		as_checksum_set_value (csum_bin, checksum_bin);
		as_checksum_set_filename (csum_bin, fw_basename);
		as_release_add_checksum (release, csum_bin);
	}

	return TRUE;
}

/**
 * asb_plugin_process_app:
 */
gboolean
asb_plugin_process_app (AsbPlugin *plugin,
			AsbPackage *pkg,
			AsbApp *app,
			const gchar *tmpdir,
			GError **error)
{
	const gchar *tmp;
	g_autofree gchar *fn = NULL;
	g_autofree gchar *inf_fn = NULL;

	/* use metainfo basename */
	tmp = as_app_get_source_file (AS_APP (app));
	if (tmp == NULL) {
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_NOT_SUPPORTED,
			     "no source_file set for %s",
			     as_app_get_id (AS_APP (app)));
		return FALSE;
	}

	/* use the .inf file to refine the application */
	inf_fn = asb_plugin_firmware_get_inf_fn (tmp);
	fn = g_build_filename (tmpdir, inf_fn, NULL);
	if (g_file_test (fn, G_FILE_TEST_EXISTS)) {
		if (!asb_plugin_firmware_refine (plugin, pkg, fn, app, tmpdir, error))
			return FALSE;
	}
	return TRUE;
}
