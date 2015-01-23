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

#include <asb-plugin.h>
#include <fnmatch.h>

/**
 * asb_plugin_get_name:
 */
const gchar *
asb_plugin_get_name (void)
{
	return "ostree";
}

/**
 * asb_plugin_add_globs:
 */
void
asb_plugin_add_globs (AsbPlugin *plugin, GPtrArray *globs)
{
	asb_plugin_add_glob (globs, "/metadata");
}

/**
 * asb_plugin_process_filename:
 */
static gboolean
asb_plugin_process_filename (const gchar *filename, AsbApp *app, GError **error)
{
	_cleanup_free_ gchar *app_runtime = NULL;
	_cleanup_free_ gchar *app_sdk = NULL;
	_cleanup_keyfile_unref_ GKeyFile *kf = NULL;
	kf = g_key_file_new ();
	if (!g_key_file_load_from_file (kf, filename, G_KEY_FILE_NONE, error))
		return FALSE;
	app_runtime = g_key_file_get_string (kf, "Application", "runtime", NULL);
	if (app_runtime != NULL)
		as_app_add_metadata (AS_APP (app), "BundleRuntime", app_runtime, -1);
	app_sdk = g_key_file_get_string (kf, "Application", "sdk", NULL);
	if (app_sdk != NULL)
		as_app_add_metadata (AS_APP (app), "BundleSDK", app_sdk, -1);
	if (g_key_file_get_string (kf, "Environment", "network", NULL))
		as_app_add_permission (AS_APP (app), "network", -1);
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

	/* look for a krunner provider */
	filelist = asb_package_get_filelist (pkg);
	for (i = 0; filelist[i] != NULL; i++) {
		_cleanup_error_free_ GError *error_local = NULL;
		_cleanup_free_ gchar *filename = NULL;
		if (!asb_plugin_match_glob ("/metadata", filelist[i]))
			continue;
		filename = g_build_filename (tmpdir, filelist[i], NULL);
		if (!asb_plugin_process_filename (filename, app, &error_local)) {
			asb_package_log (pkg,
					 ASB_PACKAGE_LOG_LEVEL_INFO,
					 "Failed to read ostree metadata file %s: %s",
					 filelist[i],
					 error_local->message);
			g_clear_error (&error_local);
		}
	}

	return TRUE;
}
