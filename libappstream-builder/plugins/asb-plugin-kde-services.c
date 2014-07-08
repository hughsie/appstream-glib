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
	return "kde-services";
}

/**
 * asb_plugin_add_globs:
 */
void
asb_plugin_add_globs (AsbPlugin *plugin, GPtrArray *globs)
{
	asb_plugin_add_glob (globs, "/usr/share/kde4/services/*.desktop");
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
		_cleanup_keyfile_unref_ GKeyFile *kf = NULL;
		_cleanup_free_ gchar *filename = NULL;
		_cleanup_free_ gchar *types = NULL;
		if (fnmatch ("/usr/share/kde4/services/*.desktop", filelist[i], 0) != 0)
			continue;
		kf = g_key_file_new ();
		filename = g_build_filename (tmpdir, filelist[i], NULL);
		if (!g_key_file_load_from_file (kf, filename, G_KEY_FILE_NONE, error))
			return FALSE;
		types = g_key_file_get_string (kf, G_KEY_FILE_DESKTOP_GROUP, "X-KDE-ServiceTypes", NULL);
		if (g_strcmp0 (types, "Plasma/Runner") == 0) {
			as_app_add_kudo_kind (AS_APP (app),
					      AS_KUDO_KIND_SEARCH_PROVIDER);
			break;
		}
	}

	return TRUE;
}
