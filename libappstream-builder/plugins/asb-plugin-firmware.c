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
 * asb_plugin_firmware_get_basename:
 */
static gchar *
asb_plugin_firmware_get_basename (const gchar *filename)
{
	gchar *basename;
	gchar *tmp;

	basename = g_path_get_basename (filename);
	tmp = g_strrstr (basename, ".inf");
	if (tmp != NULL)
		*tmp = '\0';
	return basename;
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
	_cleanup_free_ gchar *data = NULL;

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
	_cleanup_free_ gchar *checksum = NULL;
	_cleanup_free_ gchar *filename_full = NULL;
	_cleanup_free_ gchar *location_checksum = NULL;
	_cleanup_free_ gchar *fw_basename = NULL;
	_cleanup_object_unref_ AsbApp *app = NULL;

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

	/* add the checksum for the .cab file */
	checksum = asb_plugin_firmware_get_checksum (asb_package_get_filename (pkg),
						     G_CHECKSUM_SHA1,
						     error);
	if (checksum == NULL)
		return FALSE;
	release = as_app_get_release_default (AS_APP (app));
	as_release_set_checksum (release, G_CHECKSUM_SHA1, checksum, -1);

	/* for the adddata plugin; removed in asb_plugin_merge() */
	fw_basename = asb_plugin_firmware_get_basename (filename);
	if (fw_basename != NULL)
		as_app_add_metadata (AS_APP (app), "FirmwareBasename", fw_basename, -1);

	/* added so we can mirror files */
	as_release_set_filename (release, asb_package_get_basename (pkg), -1);

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

	/* remove the FirmwareBasename metadata */
	for (l = list; l != NULL; l = l->next) {
		app = AS_APP (l->data);
		if (as_app_get_id_kind (app) != AS_ID_KIND_FIRMWARE)
			continue;
		as_app_remove_metadata (app, "FirmwareBasename");
	}
}
