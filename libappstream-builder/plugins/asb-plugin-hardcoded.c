/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include <config.h>

#include <asb-plugin.h>

const gchar *
asb_plugin_get_name (void)
{
	return "hardcoded";
}

void
asb_plugin_add_globs (AsbPlugin *plugin, GPtrArray *globs)
{
	asb_plugin_add_glob (globs, "/usr/share/help/*");
	asb_plugin_add_glob (globs, "/usr/share/gnome-shell/search-providers/*");
	asb_plugin_add_glob (globs, "/usr/share/dbus-1/system-services/*.service");
	asb_plugin_add_glob (globs, "/usr/share/dbus-1/services/*.service");
}

gboolean
asb_plugin_process_app (AsbPlugin *plugin,
			AsbPackage *pkg,
			AsbApp *app,
			const gchar *tmpdir,
			GError **error)
{
	const gchar *tmp;
	GPtrArray *deps;
	gchar **filelist;
	guint i;
	g_autofree gchar *prefix = NULL;

	/* skip for addons */
	if (as_app_get_kind (AS_APP (app)) == AS_APP_KIND_ADDON)
		return TRUE;
	if (as_app_get_kind (AS_APP (app)) == AS_APP_KIND_GENERIC)
		return TRUE;

	/* look for any installed docs */
	filelist = asb_package_get_filelist (pkg);
	for (i = 0; filelist[i] != NULL; i++) {
		if (asb_plugin_match_glob ("/usr/share/help/*", filelist[i])) {
			asb_package_log (pkg,
					 ASB_PACKAGE_LOG_LEVEL_DEBUG,
					 "Auto-adding kudo UserDocs for %s",
					 as_app_get_id (AS_APP (app)));
			as_app_add_kudo_kind (AS_APP (app),
					      AS_KUDO_KIND_USER_DOCS);
			break;
		}
	}

	/* look for kudos and provides */
	prefix = g_build_filename (tmpdir, "usr", NULL);
	if (!as_app_builder_search_kudos (AS_APP (app),
					  prefix,
					  AS_APP_BUILDER_FLAG_USE_FALLBACKS,
					  error))
		return FALSE;
	if (!as_app_builder_search_provides (AS_APP (app),
					     prefix,
					     AS_APP_BUILDER_FLAG_USE_FALLBACKS,
					     error))
		return FALSE;

	/* look for a high contrast icon */
	for (i = 0; filelist[i] != NULL; i++) {
		if (asb_plugin_match_glob ("/usr/share/icons/HighContrast/*",
					   filelist[i])) {
			as_app_add_kudo_kind (AS_APP (app),
					      AS_KUDO_KIND_HIGH_CONTRAST);
			break;
		}
	}

	/* look for a modern toolkit */
	deps = asb_package_get_deps (pkg);
	for (i = 0; i < deps->len; i++) {
		tmp = g_ptr_array_index (deps, i);
		if (g_strcmp0 (tmp, "libgtk-3.so.0") == 0 ||
		    g_strcmp0 (tmp, "libQt5Core.so.5") == 0) {
			as_app_add_kudo_kind (AS_APP (app),
					      AS_KUDO_KIND_MODERN_TOOLKIT);
			break;
		}
	}

	return TRUE;
}
