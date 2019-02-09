/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2016 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include <config.h>

#include <asb-plugin.h>

const gchar *
asb_plugin_get_name (void)
{
	return "gettext";
}

void
asb_plugin_add_globs (AsbPlugin *plugin, GPtrArray *globs)
{
	asb_plugin_add_glob (globs, "/usr/share/locale/*/LC_MESSAGES/*.mo");
	asb_plugin_add_glob (globs, "/usr/share/*/translations/*.qm");
	asb_plugin_add_glob (globs, "/usr/lib64/*/locales/*.pak");
	asb_plugin_add_glob (globs, "/usr/lib64/*/langpacks/*.xpi");
}

gboolean
asb_plugin_process_app (AsbPlugin *plugin,
			AsbPackage *pkg,
			AsbApp *app,
			const gchar *tmpdir,
			GError **error)
{
	g_autofree gchar *prefix = NULL;
	GPtrArray *translations;

	/* skip for addons */
	if (as_app_get_kind (AS_APP (app)) == AS_APP_KIND_ADDON)
		return TRUE;
	if (as_app_get_kind (AS_APP (app)) == AS_APP_KIND_GENERIC)
		return TRUE;

	/* auto-add this */
	translations = as_app_get_translations (AS_APP (app));
	if (translations->len == 0) {
		g_autoptr(AsTranslation) translation = as_translation_new ();
		as_translation_set_id (translation, asb_package_get_name (pkg));
		as_app_add_translation (AS_APP (app), translation);
	}

	/* search for .mo files in the prefix */
	prefix = g_build_filename (tmpdir, "usr", NULL);
	return as_app_builder_search_translations (AS_APP (app), prefix, 25,
						   AS_APP_BUILDER_FLAG_USE_FALLBACKS,
						   NULL, error);
}
