/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2015 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include <config.h>
#include <string.h>
#include <fnmatch.h>

#include <asb-plugin.h>

#define __APPSTREAM_GLIB_PRIVATE_H
#include <as-utils-private.h>
#include <as-app-private.h>

const gchar *
asb_plugin_get_name (void)
{
	return "desktop";
}

void
asb_plugin_add_globs (AsbPlugin *plugin, GPtrArray *globs)
{
	asb_plugin_add_glob (globs, "/usr/share/applications/*.desktop");
	asb_plugin_add_glob (globs, "/usr/share/applications/kde4/*.desktop");
}

static gboolean
asb_plugin_desktop_refine (AsbPlugin *plugin,
			   AsbPackage *pkg,
			   const gchar *filename,
			   AsbApp *app,
			   const gchar *tmpdir,
			   GError **error)
{
	AsAppParseFlags parse_flags = AS_APP_PARSE_FLAG_USE_HEURISTICS |
				      AS_APP_PARSE_FLAG_ALLOW_VETO;
	g_autoptr(AsApp) desktop_app = NULL;
	g_autoptr(GdkPixbuf) pixbuf = NULL;

	/* use GenericName fallback */
	if (asb_context_get_flag (plugin->ctx, ASB_CONTEXT_FLAG_USE_FALLBACKS))
		parse_flags |= AS_APP_PARSE_FLAG_USE_FALLBACKS;

	/* create app */
	desktop_app = as_app_new ();
	if (!as_app_parse_file (desktop_app, filename, parse_flags, error))
		return FALSE;

	/* copy all metadata */
	as_app_subsume_full (AS_APP (app), desktop_app,
			     AS_APP_SUBSUME_FLAG_NO_OVERWRITE |
			     AS_APP_SUBSUME_FLAG_MERGE);

	return TRUE;
}

gboolean
asb_plugin_process_app (AsbPlugin *plugin,
			AsbPackage *pkg,
			AsbApp *app,
			const gchar *tmpdir,
			GError **error)
{
	AsLaunchable *launchable;
	gboolean found = FALSE;
	guint i;
	g_autoptr(GString) desktop_basename = NULL;
	const gchar *app_dirs[] = {
		"/usr/share/applications",
		"/usr/share/applications/kde4",
		NULL };

	/* get the (optional) launchable to get the name of the desktop file */
	launchable = as_app_get_launchable_by_kind (AS_APP (app), AS_LAUNCHABLE_KIND_DESKTOP_ID);
	if (launchable != NULL) {
		desktop_basename = g_string_new (as_launchable_get_value (launchable));
	} else {
		desktop_basename = g_string_new (as_app_get_id (AS_APP (app)));
		if (!g_str_has_suffix (desktop_basename->str, ".desktop"))
			g_string_append (desktop_basename, ".desktop");
	}

	/* use the .desktop file to refine the application */
	for (i = 0; app_dirs[i] != NULL; i++) {
		g_autofree gchar *fn = NULL;
		fn = g_build_filename (tmpdir,
				       app_dirs[i],
				       desktop_basename->str,
				       NULL);
		if (g_file_test (fn, G_FILE_TEST_EXISTS)) {
			if (!asb_plugin_desktop_refine (plugin, pkg, fn,
							app, tmpdir, error))
				return FALSE;
			found = TRUE;
		}
	}

	/* required */
	if (!found && as_app_get_kind (AS_APP (app)) == AS_APP_KIND_DESKTOP) {
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_FAILED,
			     "a desktop file is required for %s",
			     as_app_get_id (AS_APP (app)));
		return FALSE;
	}

	return TRUE;
}
