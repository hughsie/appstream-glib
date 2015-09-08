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
 * _asb_plugin_check_filename:
 */
static gboolean
_asb_plugin_check_filename (const gchar *filename)
{
	if (asb_plugin_match_glob ("*.inf", filename))
		return TRUE;
	return FALSE;
}

/**
 * asb_plugin_check_filename:
 */
gboolean
asb_plugin_check_filename (AsbPlugin *plugin, const gchar *filename)
{
	return _asb_plugin_check_filename (filename);
}

/**
 * asb_plugin_firmware_get_metainfo_fn:
 */
static gchar *
asb_plugin_firmware_get_metainfo_fn (const gchar *filename)
{
	gchar *basename;
	gchar *tmp;

	basename = g_path_get_basename (filename);
	tmp = g_strrstr (basename, ".inf");
	if (tmp != NULL)
		*tmp = '\0';
	return g_strdup_printf ("%s.metainfo.xml", basename);
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
 * asb_plugin_process_filename:
 */
static gboolean
asb_plugin_process_filename (AsbPlugin *plugin,
			     AsbPackage *pkg,
			     const gchar *filename,
			     GList **apps,
			     const gchar *tmpdir,
			     GError **error)
{
	AsRelease *release;
	GError *error_local = NULL;
	const gchar *fw_basename = NULL;
	g_autofree gchar *checksum = NULL;
	g_autofree gchar *filename_full = NULL;
	g_autofree gchar *location_checksum = NULL;
	g_autofree gchar *metainfo_fn = NULL;
	g_autoptr(AsbApp) app = NULL;
	g_autoptr(AsChecksum) csum = NULL;

	/* parse */
	filename_full = g_build_filename (tmpdir, filename, NULL);
	app = asb_app_new (pkg, NULL);
	if (!as_app_parse_file (AS_APP (app), filename_full,
				AS_APP_PARSE_FLAG_NONE, &error_local)) {
		g_set_error_literal (error,
				     ASB_PLUGIN_ERROR,
				     ASB_PLUGIN_ERROR_NOT_SUPPORTED,
				     error_local->message);
		return FALSE;
	}

	/* get the default release, creating if required */
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

	/* for the adddata plugin; removed in asb_plugin_merge() */
	metainfo_fn = asb_plugin_firmware_get_metainfo_fn (filename);
	if (metainfo_fn != NULL)
		as_app_add_metadata (AS_APP (app), "MetainfoBasename", metainfo_fn);

	/* set the internal checksum */
	fw_basename = as_app_get_metadata_item (AS_APP (app), "FirmwareBasename");
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

	asb_plugin_add_app (apps, AS_APP (app));
	return TRUE;
}

/**
 * asb_plugin_process:
 */
GList *
asb_plugin_process (AsbPlugin *plugin,
		    AsbPackage *pkg,
		    const gchar *tmpdir,
		    GError **error)
{
	gboolean ret;
	GError *error_local = NULL;
	GList *apps = NULL;
	guint i;
	gchar **filelist;

	filelist = asb_package_get_filelist (pkg);
	for (i = 0; filelist[i] != NULL; i++) {
		if (!_asb_plugin_check_filename (filelist[i]))
			continue;
		ret = asb_plugin_process_filename (plugin,
						   pkg,
						   filelist[i],
						   &apps,
						   tmpdir,
						   &error_local);
		if (!ret) {
			asb_package_log (pkg,
					 ASB_PACKAGE_LOG_LEVEL_INFO,
					 "Failed to process %s: %s",
					 filelist[i],
					 error_local->message);
			g_clear_error (&error_local);
		}
	}

	/* no desktop files we care about */
	if (apps == NULL) {
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_FAILED,
			     "nothing interesting in %s",
			     asb_package_get_basename (pkg));
		return NULL;
	}
	return apps;
}

/**
 * asb_plugin_merge:
 */
void
asb_plugin_merge (AsbPlugin *plugin, GList *list)
{
	AsApp *app;
	GList *l;

	/* remove the MetainfoBasename metadata */
	for (l = list; l != NULL; l = l->next) {
		app = AS_APP (l->data);
		if (as_app_get_id_kind (app) != AS_ID_KIND_FIRMWARE)
			continue;
		as_app_remove_metadata (app, "MetainfoBasename");
		as_app_remove_metadata (app, "FirmwareBasename");
	}
}
