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

#include <config.h>
#include <fnmatch.h>

#include <asb-plugin.h>

/**
 * asb_plugin_get_name:
 */
const gchar *
asb_plugin_get_name (void)
{
	return "dbus";
}

/**
 * asb_plugin_add_globs:
 */
void
asb_plugin_add_globs (AsbPlugin *plugin, GPtrArray *globs)
{
	asb_plugin_add_glob (globs, "/usr/share/dbus-1/system-services/*.service");
	asb_plugin_add_glob (globs, "/usr/share/dbus-1/services/*.service");
}

/**
 * _asb_plugin_check_filename_system:
 */
static gboolean
_asb_plugin_check_filename_system (const gchar *filename)
{
	if (fnmatch ("/usr/share/dbus-1/system-services/*.service", filename, 0) == 0)
		return TRUE;
	return FALSE;
}

/**
 * _asb_plugin_check_filename_session:
 */
static gboolean
_asb_plugin_check_filename_session (const gchar *filename)
{
	if (fnmatch ("/usr/share/dbus-1/services/*.service", filename, 0) == 0)
		return TRUE;
	return FALSE;
}

/**
 * asb_plugin_process_dbus:
 */
static gboolean
asb_plugin_process_dbus (AsbApp *app,
			 const gchar *tmpdir,
			 const gchar *filename,
			 gboolean is_system,
			 GError **error)
{
	_cleanup_free_ gchar *filename_full = NULL;
	_cleanup_free_ gchar *name = NULL;
	_cleanup_keyfile_unref_ GKeyFile *kf = NULL;
	_cleanup_object_unref_ AsProvide *provide = NULL;

	/* load file */
	filename_full = g_build_filename (tmpdir, filename, NULL);
	kf = g_key_file_new ();
	if (!g_key_file_load_from_file (kf, filename_full, G_KEY_FILE_NONE, error))
		return FALSE;
	name = g_key_file_get_string (kf, "D-BUS Service", "Name", error);
	if (name == NULL)
		return FALSE;

	/* add provide */
	provide = as_provide_new ();
	as_provide_set_kind (provide, is_system ? AS_PROVIDE_KIND_DBUS_SYSTEM :
						  AS_PROVIDE_KIND_DBUS);
	as_provide_set_value (provide, name, -1);
	as_app_add_provide (AS_APP (app), provide);
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
	gchar **filelist;
	guint i;

	/* look for any GIR files */
	filelist = asb_package_get_filelist (pkg);
	for (i = 0; filelist[i] != NULL; i++) {
		if (_asb_plugin_check_filename_system (filelist[i])) {
			if (!asb_plugin_process_dbus (app, tmpdir, filelist[i],
						      TRUE, error))
				return FALSE;
		} else if (_asb_plugin_check_filename_session (filelist[i])) {
			if (!asb_plugin_process_dbus (app, tmpdir, filelist[i],
						      FALSE, error))
				return FALSE;
		}
	}
	return TRUE;
}
