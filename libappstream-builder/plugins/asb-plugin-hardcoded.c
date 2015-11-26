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

struct AsbPluginPrivate {
	GPtrArray	*project_groups;
};

/**
 * asb_plugin_get_name:
 */
const gchar *
asb_plugin_get_name (void)
{
	return "hardcoded";
}

/**
 * asb_plugin_add_globs:
 */
void
asb_plugin_add_globs (AsbPlugin *plugin, GPtrArray *globs)
{
	asb_plugin_add_glob (globs, "/usr/share/help/*");
	asb_plugin_add_glob (globs, "/usr/share/gnome-shell/search-providers/*");
}

/**
 * asb_plugin_initialize:
 */
void
asb_plugin_initialize (AsbPlugin *plugin)
{
	plugin->priv = ASB_PLUGIN_GET_PRIVATE (AsbPluginPrivate);
	plugin->priv->project_groups = asb_glob_value_array_new ();

	/* this is a heuristic */
	g_ptr_array_add (plugin->priv->project_groups,
			 asb_glob_value_new ("http*://*.gnome.org*", "GNOME"));
	g_ptr_array_add (plugin->priv->project_groups,
			 asb_glob_value_new ("http://gnome-*.sourceforge.net/", "GNOME"));
	g_ptr_array_add (plugin->priv->project_groups,
			 asb_glob_value_new ("http*://*.kde.org*", "KDE"));
	g_ptr_array_add (plugin->priv->project_groups,
			 asb_glob_value_new ("http://*kde-apps.org/*", "KDE"));
	g_ptr_array_add (plugin->priv->project_groups,
			 asb_glob_value_new ("http://*xfce.org*", "XFCE"));
	g_ptr_array_add (plugin->priv->project_groups,
			 asb_glob_value_new ("http://lxde.org*", "LXDE"));
	g_ptr_array_add (plugin->priv->project_groups,
			 asb_glob_value_new ("http://pcmanfm.sourceforge.net/*", "LXDE"));
	g_ptr_array_add (plugin->priv->project_groups,
			 asb_glob_value_new ("http://lxde.sourceforge.net/*", "LXDE"));
	g_ptr_array_add (plugin->priv->project_groups,
			 asb_glob_value_new ("http://*mate-desktop.org*", "MATE"));
	g_ptr_array_add (plugin->priv->project_groups,
			 asb_glob_value_new ("http://*enlightenment.org*", "Enlightenment"));
}

/**
 * asb_plugin_destroy:
 */
void
asb_plugin_destroy (AsbPlugin *plugin)
{
	g_ptr_array_unref (plugin->priv->project_groups);
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
	const gchar *tmp;
	AsRelease *release;
	GPtrArray *deps;
	gchar **filelist;
	GPtrArray *releases;
	guint i;
	gint64 secs;
	guint days;

	/* add extra categories */
	tmp = as_app_get_id (AS_APP (app));
	if (g_strcmp0 (tmp, "nautilus.desktop") == 0) {
		as_app_add_category (AS_APP (app), "System");
		asb_package_log (pkg,
				 ASB_PACKAGE_LOG_LEVEL_DEBUG,
				 "Auto-adding category System for %s",
				 as_app_get_id (AS_APP (app)));
	}

	/* add extra project groups */
	if (g_strcmp0 (tmp, "xfdashboard.desktop") == 0) {
		as_app_set_project_group (AS_APP (app), "XFCE");
		asb_package_log (pkg,
				 ASB_PACKAGE_LOG_LEVEL_DEBUG,
				 "Auto-adding project group XFCE for %s",
				 as_app_get_id (AS_APP (app)));
	}

	/* use the URL to guess the project group */
	tmp = asb_package_get_url (pkg);
	if (as_app_get_project_group (AS_APP (app)) == NULL && tmp != NULL) {
		tmp = asb_glob_value_search (plugin->priv->project_groups, tmp);
		if (tmp != NULL)
			as_app_set_project_group (AS_APP (app), tmp);
	}

	/* use summary to guess the project group */
	tmp = as_app_get_comment (AS_APP (app), NULL);
	if (tmp != NULL && g_strstr_len (tmp, -1, "for KDE") != NULL)
		as_app_set_project_group (AS_APP (app), "KDE");

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

	/* look for a shell search provider */
	for (i = 0; filelist[i] != NULL; i++) {
		if (asb_plugin_match_glob ("/usr/share/gnome-shell/search-providers/*",
					   filelist[i])) {
			asb_package_log (pkg,
					 ASB_PACKAGE_LOG_LEVEL_DEBUG,
					 "Auto-adding kudo SearchProvider for %s",
					 as_app_get_id (AS_APP (app)));
			as_app_add_kudo_kind (AS_APP (app),
					      AS_KUDO_KIND_SEARCH_PROVIDER);
			break;
		}
	}

	/* look for a high contrast icon */
	for (i = 0; filelist[i] != NULL; i++) {
		if (asb_plugin_match_glob ("/usr/share/icons/HighContrast/*",
					   filelist[i])) {
			as_app_add_kudo_kind (AS_APP (app),
					      AS_KUDO_KIND_HIGH_CONTRAST);
			break;
		}
		if (asb_plugin_match_glob ("/usr/share/icons/hicolor/symbolic/apps/*.svg",
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

	/* has the application been updated in the last year */
	releases = as_app_get_releases (AS_APP (app));
	if (asb_context_get_api_version (plugin->ctx) < 0.8) {
		for (i = 0; i < releases->len; i++) {
			release = g_ptr_array_index (releases, i);
			secs = (g_get_real_time () / G_USEC_PER_SEC) -
				as_release_get_timestamp (release);
			days = secs / (60 * 60 * 24);
			if (secs > 0 && days < 365) {
				as_app_add_metadata (AS_APP (app),
						     "X-Kudo-RecentRelease", "");
				break;
			}
		}
	}

	/* has there been no upstream version recently */
	if (releases->len > 0 &&
	    as_app_get_id_kind (AS_APP (app)) == AS_ID_KIND_DESKTOP &&
	    !asb_context_get_flag (plugin->ctx, ASB_CONTEXT_FLAG_IGNORE_DEAD_UPSTREAM)) {
		release = g_ptr_array_index (releases, 0);
		secs = (g_get_real_time () / G_USEC_PER_SEC) -
			as_release_get_timestamp (release);
		days = secs / (60 * 60 * 24);
		if (secs > 0 && days > 365 * 5) {
			asb_package_log (asb_app_get_package (app),
					 ASB_PACKAGE_LOG_LEVEL_WARNING,
					 "Dead upstream for > %i years", 5);
		}
	}

	/* a ConsoleOnly category means we require AppData */
	if (as_app_has_category (AS_APP(app), "ConsoleOnly")) {
		asb_package_log (pkg,
				 ASB_PACKAGE_LOG_LEVEL_DEBUG,
				 "Auto-adding veto ConsoleOnly for %s",
				 as_app_get_id (AS_APP (app)));
		as_app_add_veto (AS_APP (app), "ConsoleOnly");
	}

	/* no categories means veto */
//	if (as_app_get_id_kind (AS_APP (app)) == AS_ID_KIND_DESKTOP &&
//	    as_app_get_categories(AS_APP(app))->len == 0)
//		as_app_add_veto (AS_APP (app), "no Categories");

	return TRUE;
}
