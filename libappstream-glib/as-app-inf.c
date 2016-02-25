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
#include "as-inf.h"
#include "as-utils-private.h"

#define AS_APP_INF_CLASS_GUID_FIRMWARE	"f2e7dd72-6468-4e36-b6f1-6488f42c1b52"

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
			g_string_append_c (id, g_ascii_tolower (guid[i]));
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
	g_autoptr(GError) error_local = NULL;
	g_autofree gchar *appstream_id = NULL;
	g_autofree gchar *catalog_basename = NULL;
	g_autofree gchar *class_guid = NULL;
	g_autofree gchar *class_guid_unsafe = NULL;
	g_autofree gchar *class = NULL;
	g_autofree gchar *comment = NULL;
	g_autofree gchar *display_version = NULL;
	g_autofree gchar *filename_full = NULL;
	g_autofree gchar *firmware_basename = NULL;
	g_autofree gchar *guid = NULL;
	g_autofree gchar *provide_guid = NULL;
	g_autofree gchar *location_checksum = NULL;
	g_autofree gchar *name = NULL;
	g_autofree gchar *srcpkg = NULL;
	g_autofree gchar *vendor = NULL;
	g_autofree gchar *version = NULL;
	g_autofree gchar *version_parsed = NULL;
	g_autoptr(GKeyFile) kf = NULL;
	g_autoptr(AsChecksum) csum = NULL;
	g_autoptr(AsIcon) icon = NULL;
	g_autoptr(AsProvide) provide = NULL;
	g_autoptr(AsRelease) release = NULL;
	g_auto(GStrv) source_keys = NULL;

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
	as_app_set_kind (app, AS_APP_KIND_FIRMWARE);

	/* get the Class GUID */
	class_guid_unsafe = g_key_file_get_string (kf, "Version", "ClassGuid", NULL);
	if (class_guid_unsafe == NULL) {
		g_set_error_literal (error,
				     AS_APP_ERROR,
				     AS_APP_ERROR_INVALID_TYPE,
				     "ClassGuid is missing");
		return FALSE;
	}
	class_guid = as_app_parse_inf_sanitize_guid (class_guid_unsafe);
	if (g_strcmp0 (class_guid, AS_APP_INF_CLASS_GUID_FIRMWARE) != 0) {
		g_set_error (error,
			     AS_APP_ERROR,
			     AS_APP_ERROR_INVALID_TYPE,
			     "ClassGuid is invalid, expected %s, got %s",
			     AS_APP_INF_CLASS_GUID_FIRMWARE,
			     class_guid);
		return FALSE;
	}

	/* get the ESRT GUID */
	guid = g_key_file_get_string (kf,
				      "Firmware_AddReg",
				      "HKR_FirmwareId",
				      NULL);
	if (guid == NULL) {
		g_set_error_literal (error,
				     AS_APP_ERROR,
				     AS_APP_ERROR_INVALID_TYPE,
				     "HKR->FirmwareId missing from [Firmware_AddReg]");
		return FALSE;
	}

	/* get the version */
	version = g_key_file_get_string (kf,
					"Firmware_AddReg",
					"HKR_FirmwareVersion_0x00010001",
					NULL);
	if (version == NULL) {
		g_set_error_literal (error,
				     AS_APP_ERROR,
				     AS_APP_ERROR_INVALID_TYPE,
				     "HKR->FirmwareVersion missing from [Firmware_AddReg]");
		return FALSE;
	}

	/* convert if required */
	version_parsed = as_utils_version_parse (version);

	/* add the GUID as a provide */
	provide_guid = as_app_parse_inf_sanitize_guid (guid);
	if (provide_guid != NULL) {
		provide = as_provide_new ();
		as_provide_set_kind (provide, AS_PROVIDE_KIND_FIRMWARE_FLASHED);
		as_provide_set_value (provide, provide_guid);
		as_app_add_provide (AS_APP (app), provide);
	}

	/* get the ID, which might be missing */
	appstream_id = g_key_file_get_string (kf, "Version", "AppstreamId", NULL);
	if (appstream_id != NULL) {
		g_debug ("Using AppstreamId as ID");
		as_app_set_id (app, appstream_id);
	} else {
		as_app_set_id (app, provide_guid);
	}

	/* get vendor */
	vendor = g_key_file_get_string (kf, "Version", "Provider", NULL);
	if (vendor == NULL)
		vendor = g_key_file_get_string (kf, "Version", "MfgName", NULL);
	if (vendor != NULL)
		as_app_set_developer_name (app, NULL, vendor);

	/* get name */
	name = g_key_file_get_string (kf, "Strings", "FirmwareDesc", NULL);
	if (name != NULL)
		as_app_set_name (app, NULL, name);

	/* get comment */
	comment = g_key_file_get_string (kf, "SourceDisksNames", "1", NULL);
	if (comment == NULL) /* FIXME: is a hack */
		comment = g_key_file_get_string (kf, "Strings", "DiskName", NULL);
	if (comment != NULL)
		as_app_set_comment (app, NULL, comment);

	/* parse the DriverVer */
	display_version = as_inf_get_driver_version (kf, &timestamp, &error_local);
	if (display_version == NULL) {
		if (g_error_matches (error_local,
				     AS_INF_ERROR,
				     AS_INF_ERROR_NOT_FOUND)) {
			g_clear_error (&error_local);
		} else {
			g_set_error_literal (error,
					     AS_APP_ERROR,
					     AS_APP_ERROR_FAILED,
					     error_local->message);
			return FALSE;
		}
	}

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

	/* this is for display only */
	if (display_version != NULL)
		as_app_add_metadata (app, "DisplayVersion", display_version);

	/* add a release with no real description */
	release = as_release_new ();
	as_release_set_version (release, version_parsed);
	as_release_set_timestamp (release, timestamp);
	csum = as_checksum_new ();
	as_checksum_set_filename (csum, firmware_basename);
	as_checksum_set_target (csum, AS_CHECKSUM_TARGET_CONTENT);
	as_release_add_checksum (release, csum);
	as_app_add_release (app, release);

	/* add icon */
	icon = as_icon_new ();
	as_icon_set_kind (icon, AS_ICON_KIND_STOCK);
	as_icon_set_name (icon, "application-x-executable");
	as_app_add_icon (app, icon);

	return TRUE;
}
