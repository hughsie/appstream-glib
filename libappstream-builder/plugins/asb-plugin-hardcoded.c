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
 * asb_plugin_hardcoded_sort_screenshots_cb:
 */
static gint
asb_plugin_hardcoded_sort_screenshots_cb (gconstpointer a, gconstpointer b)
{
	return g_strcmp0 ((gchar *) a, (gchar *) b);
}

/**
 * asb_plugin_hardcoded_add_screenshots:
 */
static gboolean
asb_plugin_hardcoded_add_screenshots (AsbApp *app,
				      const gchar *location,
				      GError **error)
{
	const gchar *tmp;
	gboolean ret = TRUE;
	GList *l;
	GList *list = NULL;
	_cleanup_dir_close_ GDir *dir;

	/* scan for files */
	dir = g_dir_open (location, 0, error);
	if (dir == NULL) {
		ret = FALSE;
		goto out;
	}
	while ((tmp = g_dir_read_name (dir)) != NULL) {
		if (!g_str_has_suffix (tmp, ".png"))
			continue;
		list = g_list_prepend (list, g_build_filename (location, tmp, NULL));
	}
	list = g_list_sort (list, asb_plugin_hardcoded_sort_screenshots_cb);
	for (l = list; l != NULL; l = l->next) {
		tmp = l->data;
		asb_package_log (asb_app_get_package (app),
				 ASB_PACKAGE_LOG_LEVEL_DEBUG,
				 "Adding extra screenshot: %s", tmp);
		ret = asb_app_add_screenshot_source (app, tmp, error);
		if (!ret)
			goto out;
	}
	as_app_add_metadata (AS_APP (app), "DistroScreenshots", NULL, -1);
out:
	g_list_free_full (list, g_free);
	return ret;
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
	gchar **deps;
	gchar **filelist;
	GPtrArray *releases;
	guint i;
	gint64 secs;
	guint days;

	/* add extra categories */
	tmp = as_app_get_id_full (AS_APP (app));
	if (g_strcmp0 (tmp, "0install.desktop") == 0)
		as_app_add_category (AS_APP (app), "System", -1);
	if (g_strcmp0 (tmp, "alacarte.desktop") == 0)
		as_app_add_category (AS_APP (app), "System", -1);
	if (g_strcmp0 (tmp, "deja-dup.desktop") == 0)
		as_app_add_category (AS_APP (app), "Utility", -1);
	if (g_strcmp0 (tmp, "gddccontrol.desktop") == 0)
		as_app_add_category (AS_APP (app), "System", -1);
	if (g_strcmp0 (tmp, "nautilus.desktop") == 0)
		as_app_add_category (AS_APP (app), "System", -1);
	if (g_strcmp0 (tmp, "pessulus.desktop") == 0)
		as_app_add_category (AS_APP (app), "System", -1);
	if (g_strcmp0 (tmp, "pmdefaults.desktop") == 0)
		as_app_add_category (AS_APP (app), "System", -1);
	if (g_strcmp0 (tmp, "fwfstab.desktop") == 0)
		as_app_add_category (AS_APP (app), "System", -1);
	if (g_strcmp0 (tmp, "bmpanel2cfg.desktop") == 0)
		as_app_add_category (AS_APP (app), "System", -1);

	/* add extra project groups */
	if (g_strcmp0 (tmp, "nemo.desktop") == 0)
		as_app_set_project_group (AS_APP (app), "Cinnamon", -1);
	if (g_strcmp0 (tmp, "xfdashboard.desktop") == 0)
		as_app_set_project_group (AS_APP (app), "XFCE", -1);

	/* use the URL to guess the project group */
	tmp = asb_package_get_url (pkg);
	if (as_app_get_project_group (AS_APP (app)) == NULL && tmp != NULL) {
		tmp = asb_glob_value_search (plugin->priv->project_groups, tmp);
		if (tmp != NULL)
			as_app_set_project_group (AS_APP (app), tmp, -1);
	}

	/* use summary to guess the project group */
	tmp = as_app_get_comment (AS_APP (app), NULL);
	if (tmp != NULL && g_strstr_len (tmp, -1, "for KDE") != NULL)
		as_app_set_project_group (AS_APP (app), "KDE", -1);

	/* look for any installed docs */
	filelist = asb_package_get_filelist (pkg);
	for (i = 0; filelist[i] != NULL; i++) {
		if (g_str_has_prefix (filelist[i],
				      "/usr/share/help/")) {
			as_app_add_kudo_kind (AS_APP (app),
					      AS_KUDO_KIND_USER_DOCS);
			break;
		}
	}

	/* look for a shell search provider */
	for (i = 0; filelist[i] != NULL; i++) {
		if (g_str_has_prefix (filelist[i],
				      "/usr/share/gnome-shell/search-providers/")) {
			as_app_add_kudo_kind (AS_APP (app),
					      AS_KUDO_KIND_SEARCH_PROVIDER);
			break;
		}
	}

	/* look for a modern toolkit */
	deps = asb_package_get_deps (pkg);
	for (i = 0; deps != NULL && deps[i] != NULL; i++) {
		if (g_strcmp0 (deps[i], "libgtk-3.so.0") == 0 ||
		    g_strcmp0 (deps[i], "libQt5Core.so.5") == 0) {
			as_app_add_kudo_kind (AS_APP (app),
					      AS_KUDO_KIND_MODERN_TOOLKIT);
			break;
		}
	}

	/* look for ancient toolkits */
	for (i = 0; deps != NULL && deps[i] != NULL; i++) {
		if (g_strcmp0 (deps[i], "libgtk-1.2.so.0") == 0) {
			as_app_add_veto (AS_APP (app), "Uses obsolete GTK1 toolkit");
			break;
		}
		if (g_strcmp0 (deps[i], "libglib-1.2.so.0") == 0) {
			as_app_add_veto (AS_APP (app), "Uses obsolete GLib library");
			break;
		}
		if (g_strcmp0 (deps[i], "libqt-mt.so.3") == 0) {
			as_app_add_veto (AS_APP (app), "Uses obsolete QT3 toolkit");
			break;
		}
		if (g_strcmp0 (deps[i], "liblcms.so.1") == 0) {
			as_app_add_veto (AS_APP (app), "Uses obsolete LCMS library");
			break;
		}
		if (g_strcmp0 (deps[i], "libelektra.so.4") == 0) {
			as_app_add_veto (AS_APP (app), "Uses obsolete Elektra library");
			break;
		}
		if (g_strcmp0 (deps[i], "libXt.so.6") == 0) {
			asb_app_add_requires_appdata (app, "Uses obsolete X11 toolkit");
			break;
		}
		if (g_strcmp0 (deps[i], "wine-core") == 0) {
			asb_app_add_requires_appdata (app, "Uses wine");
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
						     "X-Kudo-RecentRelease", "", -1);
				break;
			}
		}
	}

	/* has there been no upstream version recently */
	if (releases->len > 0 &&
	    as_app_get_id_kind (AS_APP (app)) == AS_ID_KIND_DESKTOP) {
		release = g_ptr_array_index (releases, 0);
		secs = (g_get_real_time () / G_USEC_PER_SEC) -
			as_release_get_timestamp (release);
		days = secs / (60 * 60 * 24);
		/* we need AppData if the app needs saving */
		if (secs > 0 && days > 365 * 5) {
			asb_app_add_requires_appdata (app,
				"Dead upstream for > %i years", 5);
		}
	}

	/* do any extra screenshots exist */
	tmp = asb_package_get_config (pkg, "ScreenshotsExtra");
	if (tmp != NULL) {
		_cleanup_free_ gchar *dirname = NULL;
		dirname = g_build_filename (tmp, as_app_get_id (AS_APP (app)), NULL);
		if (g_file_test (dirname, G_FILE_TEST_EXISTS)) {
			if (!asb_plugin_hardcoded_add_screenshots (app, dirname, error))
				return FALSE;
		}
	}

	/* a ConsoleOnly category means we require AppData */
	if (as_app_has_category (AS_APP(app), "ConsoleOnly"))
		asb_app_add_requires_appdata (app, "ConsoleOnly");

	/* no categories means we require AppData */
	if (as_app_get_id_kind (AS_APP (app)) == AS_ID_KIND_DESKTOP &&
	    as_app_get_categories(AS_APP(app))->len == 0)
		asb_app_add_requires_appdata (app, "no Categories");

	return TRUE;
}
