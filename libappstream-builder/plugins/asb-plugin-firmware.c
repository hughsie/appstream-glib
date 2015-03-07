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
 * asb_plugin_firmware_sanitize_guid:
 */
static gchar *
asb_plugin_firmware_sanitize_guid (const gchar *id)
{
	GString *id_new;
	guint i;

	id_new = g_string_sized_new (strlen (id));

	for (i = 0; id[i] != '\0'; i++) {
		if (g_ascii_isalnum (id[i]) || id[i] == '-')
			g_string_append_c (id_new, id[i]);
	}
	return g_string_free (id_new, FALSE);
}

/**
 * asb_plugin_firmware_get_source_package:
 */
static gchar *
asb_plugin_firmware_get_source_package (const gchar *filename)
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
	guint64 timestamp;
	guint i;
	_cleanup_free_ gchar *checksum = NULL;
	_cleanup_free_ gchar *class = NULL;
	_cleanup_free_ gchar *comment = NULL;
	_cleanup_free_ gchar *filename_full = NULL;
	_cleanup_free_ gchar *id_new = NULL;
	_cleanup_free_ gchar *id = NULL;
	_cleanup_free_ gchar *location_checksum = NULL;
	_cleanup_free_ gchar *location_url = NULL;
	_cleanup_free_ gchar *name = NULL;
	_cleanup_free_ gchar *srcpkg = NULL;
	_cleanup_free_ gchar *vendor = NULL;
	_cleanup_free_ gchar *version = NULL;
	_cleanup_keyfile_unref_ GKeyFile *kf = NULL;
	_cleanup_object_unref_ AsbApp *app = NULL;
	_cleanup_object_unref_ AsIcon *icon = NULL;
	_cleanup_object_unref_ AsProvide *provide = NULL;
	_cleanup_object_unref_ AsRelease *release = NULL;

	/* fix up the inf file */
	filename_full = g_build_filename (tmpdir, filename, NULL);
	kf = g_key_file_new ();
	if (!as_inf_load_file (kf, filename_full, AS_INF_LOAD_FLAG_NONE, error))
		return FALSE;

	/* parse */
	class = g_key_file_get_string (kf, "Version", "Class", NULL);
	if (class == NULL) {
		g_set_error_literal (error,
				     ASB_PLUGIN_ERROR,
				     ASB_PLUGIN_ERROR_NOT_SUPPORTED,
				     "Driver class is missing");
		return FALSE;
	}
	if (g_strcmp0 (class, "Firmware") != 0) {
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_NOT_SUPPORTED,
			     "Driver class is '%s', not 'Firmware'", class);
		return FALSE;
	}

	/* get the GUID */
	id = g_key_file_get_string (kf, "Version", "ClassGuid", NULL);
	if (id == NULL) {
		g_set_error_literal (error,
				     ASB_PLUGIN_ERROR,
				     ASB_PLUGIN_ERROR_NOT_SUPPORTED,
				     "Driver ID is missing");
		return FALSE;
	}

	/* strip any curley brackets */
	id_new = asb_plugin_firmware_sanitize_guid (id);
	app = asb_app_new (pkg, id_new);
	as_app_set_id_kind (AS_APP (app), AS_ID_KIND_FIRMWARE);

	/* add localizable strings */
	vendor = g_key_file_get_string (kf, "Version", "Provider", NULL);
	if (vendor == NULL)
		vendor = g_key_file_get_string (kf, "Version", "MfgName", NULL);
	if (vendor == NULL)
		vendor = g_key_file_get_string (kf, "Strings", "Provider", NULL);
	if (vendor != NULL)
		as_app_set_developer_name (AS_APP (app), NULL, vendor, -1);
	name = g_key_file_get_string (kf, "Strings", "FirmwareDesc", NULL);
	if (name != NULL)
		as_app_set_name (AS_APP (app), NULL, name, -1);
	comment = g_key_file_get_string (kf, "SourceDisksNames", "1", NULL);
	if (comment == NULL)
		comment = g_key_file_get_string (kf, "Strings", "DiskName", NULL);
	if (comment != NULL)
		as_app_set_comment (AS_APP (app), NULL, comment, -1);

	/* parse the DriverVer */
	version = as_inf_get_driver_version (kf, &timestamp, error);
	if (version == NULL)
		return FALSE;

	/* add a release with no real description */
	release = as_release_new ();
	as_release_set_version (release, version, -1);
	as_release_set_timestamp (release, timestamp);
	as_app_add_release (AS_APP (app), release);

	/* this is a Linux extension */
	location_url = g_key_file_get_string (kf, "Location", "URLs", NULL);
	if (location_url != NULL) {
		_cleanup_strv_free_ gchar **location_urls = NULL;
		location_urls = g_strsplit (location_url, ",", -1);
		for (i = 0; location_urls[i] != NULL; i++)
			as_release_add_location (release, location_urls[i], -1);
	}

	/* checksum the .cab file */
	checksum = asb_plugin_firmware_get_checksum (asb_package_get_filename (pkg),
						     G_CHECKSUM_SHA1,
						     error);
	if (checksum == NULL)
		return FALSE;
	as_release_set_checksum (release, G_CHECKSUM_SHA1, checksum, -1);

	/* this is a hack */
	srcpkg = asb_plugin_firmware_get_source_package (filename);
	if (srcpkg != NULL)
		asb_package_set_source_pkgname (pkg, srcpkg);

	/* add icon */
	icon = as_icon_new ();
	as_icon_set_kind (icon, AS_ICON_KIND_STOCK);
	as_icon_set_name (icon, "application-x-executable", -1);
	as_app_add_icon (AS_APP (app), icon);

	asb_plugin_add_app (apps, AS_APP (app));

	/* add provide */
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

	/* remove the source package name, which doesn't make sense */
	for (l = list; l != NULL; l = l->next) {
		app = AS_APP (l->data);
		if (as_app_get_id_kind (app) == AS_ID_KIND_FIRMWARE)
			as_app_set_source_pkgname (app, NULL, -1);
	}
}
