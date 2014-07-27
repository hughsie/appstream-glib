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

/**
 * asb_plugin_get_name:
 */
const gchar *
asb_plugin_get_name (void)
{
	return "nm";
}

/**
 * asb_plugin_add_globs:
 */
void
asb_plugin_add_globs (AsbPlugin *plugin, GPtrArray *globs)
{
	asb_plugin_add_glob (globs, "/usr/bin/*");
}

static gboolean
asb_plugin_nm_app (AsbApp *app, const gchar *filename, GError **error)
{
	gboolean ret;
	_cleanup_free_ gchar *data_err = NULL;
	_cleanup_free_ gchar *data_out = NULL;
	const gchar *argv[] = { "/usr/bin/nm",
				"--dynamic",
				"--no-sort",
				"--undefined-only",
				filename,
				NULL };

	ret = g_spawn_sync (NULL, (gchar **) argv, NULL,
#if GLIB_CHECK_VERSION(2,40,0)
			    G_SPAWN_CLOEXEC_PIPES,
#else
			    G_SPAWN_DEFAULT,
#endif
			    NULL, NULL,
			    &data_out,
			    &data_err,
			    NULL, error);
	if (!ret)
		return FALSE;
	if (g_strstr_len (data_out, -1, "gtk_application_new") != NULL)
		as_app_add_kudo_kind (AS_APP (app), AS_KUDO_KIND_MODERN_TOOLKIT);
	if (g_strstr_len (data_out, -1, "gtk_application_set_app_menu") != NULL)
		as_app_add_kudo_kind (AS_APP (app), AS_KUDO_KIND_APP_MENU);
	if (g_strstr_len (data_out, -1, "gtk_application_get_menu_by_id") != NULL)
		as_app_add_kudo_kind (AS_APP (app), AS_KUDO_KIND_APP_MENU);
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

	filelist = asb_package_get_filelist (pkg);
	for (i = 0; filelist[i] != NULL; i++) {
		GError *error_local = NULL;
		_cleanup_free_ gchar *filename = NULL;

		if (!g_str_has_prefix (filelist[i], "/usr/bin/"))
			continue;
		if (as_app_get_metadata_item (AS_APP (app), "X-Kudo-UsesAppMenu") != NULL)
			break;
		filename = g_build_filename (tmpdir, filelist[i], NULL);
		if (!asb_plugin_nm_app (app, filename, &error_local)) {
			asb_package_log (pkg,
					 ASB_PACKAGE_LOG_LEVEL_WARNING,
					 "Failed to run nm on %s: %s",
					 filename,
					 error_local->message);
			g_clear_error (&error_local);
		}
	}
	return TRUE;
}
