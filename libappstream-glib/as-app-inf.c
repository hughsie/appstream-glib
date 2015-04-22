/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013-2015 Richard Hughes <richard@hughsie.com>
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

#include <fnmatch.h>
#include <string.h>

#include "as-app-private.h"
#include "as-cleanup.h"
#include "as-inf.h"
//#include "as-utils.h"

/**
 * as_app_parse_inf_sanitize_guid:
 */
static gchar *
as_app_parse_inf_sanitize_guid (const gchar *guid)
{
	GString *id;
	guint i;

	id = g_string_sized_new (strlen (guid));

	for (i = 0; guid[i] != '\0'; i++) {
		if (g_ascii_isalnum (guid[i]) || guid[i] == '-')
			g_string_append_c (id, guid[i]);
	}
	return g_string_free (id, FALSE);
}

/**
 * as_app_parse_inf_file:
 **/
gboolean
as_app_parse_inf_file (AsApp *app,
		       const gchar *filename,
		       AsAppParseFlags flags,
		       GError **error)
{
	guint64 timestamp;
	guint i;
	_cleanup_error_free_ GError *error_local = NULL;
	_cleanup_free_ gchar *catalog_basename = NULL;
	_cleanup_free_ gchar *class = NULL;
	_cleanup_free_ gchar *comment = NULL;
	_cleanup_free_ gchar *filename_full = NULL;
	_cleanup_free_ gchar *firmware_basename = NULL;
	_cleanup_free_ gchar *guid = NULL;
	_cleanup_free_ gchar *id = NULL;
	_cleanup_free_ gchar *location_checksum = NULL;
	_cleanup_free_ gchar *location_url = NULL;
	_cleanup_free_ gchar *name = NULL;
	_cleanup_free_ gchar *srcpkg = NULL;
	_cleanup_free_ gchar *vendor = NULL;
	_cleanup_free_ gchar *version = NULL;
	_cleanup_keyfile_unref_ GKeyFile *kf = NULL;
	_cleanup_object_unref_ AsIcon *icon = NULL;
	_cleanup_object_unref_ AsProvide *provide = NULL;
	_cleanup_object_unref_ AsRelease *release = NULL;
	_cleanup_strv_free_ gchar **source_keys = NULL;

	/* load file */
	kf = g_key_file_new ();
	if (!as_inf_load_file (kf, filename, AS_INF_LOAD_FLAG_NONE, &error_local)) {
		g_set_error (error,
			     AS_APP_ERROR,
			     AS_APP_ERROR_INVALID_TYPE,
			     "Failed to parse %s: %s",
			     filename, error_local->message);
		return FALSE;
	}

	/* get the type of .inf file */
	class = g_key_file_get_string (kf, "Version", "Class", NULL);
	if (class == NULL) {
		g_set_error_literal (error,
				     AS_APP_ERROR,
				     AS_APP_ERROR_INVALID_TYPE,
				     "Driver class is missing");
		return FALSE;
	}
	if (g_strcmp0 (class, "Firmware") != 0) {
		g_set_error (error,
			     AS_APP_ERROR,
			     AS_APP_ERROR_INVALID_TYPE,
			     "Driver class is '%s', not 'Firmware'", class);
		return FALSE;
	}
	as_app_set_id_kind (app, AS_ID_KIND_FIRMWARE);

	/* get the GUID */
	guid = g_key_file_get_string (kf, "Version", "ClassGuid", NULL);
	if (guid == NULL) {
		g_set_error_literal (error,
				     AS_APP_ERROR,
				     AS_APP_ERROR_INVALID_TYPE,
				     "Driver ID is missing");
		return FALSE;
	}

	/* strip any curley brackets */
	id = as_app_parse_inf_sanitize_guid (guid);
	as_app_set_id (app, id, -1);

	/* get vendor */
	vendor = g_key_file_get_string (kf, "Version", "Provider", NULL);
	if (vendor == NULL)
		vendor = g_key_file_get_string (kf, "Version", "MfgName", NULL);
	if (vendor == NULL) /* FIXME: is a hack */
		vendor = g_key_file_get_string (kf, "Strings", "Provider", NULL);
	if (vendor != NULL)
		as_app_set_developer_name (app, NULL, vendor, -1);

	/* get name */
	name = g_key_file_get_string (kf, "Strings", "FirmwareDesc", NULL);
	if (name != NULL)
		as_app_set_name (app, NULL, name, -1);

	/* get comment */
	comment = g_key_file_get_string (kf, "SourceDisksNames", "1", NULL);
	if (comment == NULL) /* FIXME: is a hack */
		comment = g_key_file_get_string (kf, "Strings", "DiskName", NULL);
	if (comment != NULL)
		as_app_set_comment (app, NULL, comment, -1);

	/* parse the DriverVer */
	version = as_inf_get_driver_version (kf, &timestamp, error);
	if (version == NULL)
		return FALSE;

	/* find the firmware file we're installing */
	source_keys = g_key_file_get_keys (kf, "SourceDisksFiles", NULL, NULL);
	if (source_keys != NULL && g_strv_length (source_keys) == 1) {
		firmware_basename = g_strdup (source_keys[0]);
	} else {
		firmware_basename = g_key_file_get_string (kf,
							   "Firmware_CopyFiles",
							   "value000", NULL);
	}
	if (firmware_basename == NULL) {
		g_set_error (error,
			     AS_APP_ERROR,
			     AS_APP_ERROR_INVALID_TYPE,
			     "no SourceDisksFiles or Firmware_CopyFiles");
		return FALSE;
	}
	as_app_add_metadata (app, "FirmwareBasename", firmware_basename, -1);

	/* optional */
	catalog_basename = g_key_file_get_string (kf, "Version", "CatalogFile", NULL);
	if (catalog_basename != NULL)
		as_app_add_metadata (app, "CatalogBasename", catalog_basename, -1);

	/* add a release with no real description */
	release = as_release_new ();
	as_release_set_version (release, version, -1);
	as_release_set_timestamp (release, timestamp);
	as_app_add_release (app, release);

	/* this is a Linux extension */
	location_url = g_key_file_get_string (kf, "Location", "URLs", NULL);
	if (location_url != NULL) {
		_cleanup_strv_free_ gchar **location_urls = NULL;
		location_urls = g_strsplit (location_url, ",", -1);
		for (i = 0; location_urls[i] != NULL; i++)
			as_release_add_location (release, location_urls[i], -1);
	}

	/* add icon */
	icon = as_icon_new ();
	as_icon_set_kind (icon, AS_ICON_KIND_STOCK);
	as_icon_set_name (icon, "application-x-executable", -1);
	as_app_add_icon (app, icon);

	return TRUE;
}
