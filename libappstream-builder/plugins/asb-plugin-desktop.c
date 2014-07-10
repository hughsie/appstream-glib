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
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <asb-plugin.h>

/**
 * asb_plugin_get_name:
 */
const gchar *
asb_plugin_get_name (void)
{
	return "desktop";
}

/**
 * asb_plugin_add_globs:
 */
void
asb_plugin_add_globs (AsbPlugin *plugin, GPtrArray *globs)
{
	asb_plugin_add_glob (globs, "/usr/share/applications/*.desktop");
	asb_plugin_add_glob (globs, "/usr/share/applications/kde4/*.desktop");
	asb_plugin_add_glob (globs, "/usr/share/icons/hicolor/*/apps/*");
	asb_plugin_add_glob (globs, "/usr/share/pixmaps/*");
	asb_plugin_add_glob (globs, "/usr/share/icons/*");
	asb_plugin_add_glob (globs, "/usr/share/*/icons/*");
}

/**
 * _asb_plugin_check_filename:
 */
static gboolean
_asb_plugin_check_filename (const gchar *filename)
{
	if (fnmatch ("/usr/share/applications/*.desktop", filename, 0) == 0)
		return TRUE;
	if (fnmatch ("/usr/share/applications/kde4/*.desktop", filename, 0) == 0)
		return TRUE;
	return FALSE;
}

/**
 * asb_plugin_check_filename:
 */
gboolean
asb_plugin_check_filename (AsbPlugin *plugin, const gchar *filename)
{
	return _asb_plugin_check_filename (filename);
}

/**
 * asb_app_load_icon:
 */
static GdkPixbuf *
asb_app_load_icon (const gchar *filename, const gchar *logfn, GError **error)
{
	GdkPixbuf *pixbuf = NULL;
	_cleanup_object_unref_ GdkPixbuf *pixbuf_tmp;

	/* open file in native size */
	pixbuf_tmp = gdk_pixbuf_new_from_file (filename, error);
	if (pixbuf_tmp == NULL)
		return NULL;

	/* check size */
	if (gdk_pixbuf_get_width (pixbuf_tmp) < 32 ||
	    gdk_pixbuf_get_height (pixbuf_tmp) < 32) {
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_FAILED,
			     "icon %s was too small %ix%i",
			     logfn,
			     gdk_pixbuf_get_width (pixbuf_tmp),
			     gdk_pixbuf_get_height (pixbuf_tmp));
		return NULL;
	}

	/* re-open file at correct size */
	pixbuf = gdk_pixbuf_new_from_file_at_scale (filename, 64, 64,
						    FALSE, error);
	if (pixbuf == NULL) {
		g_prefix_error (error, "Failed to open icon %s: ", logfn);
		return NULL;
	}
	return pixbuf;
}

/**
 * asb_app_find_icon:
 */
static GdkPixbuf *
asb_app_find_icon (const gchar *tmpdir, const gchar *something, GError **error)
{
	guint i;
	guint j;
	const gchar *pixmap_dirs[] = { "pixmaps", "icons", NULL };
	const gchar *supported_ext[] = { ".png",
					 ".gif",
					 ".svg",
					 ".xpm",
					 "",
					 NULL };
	const gchar *sizes[] = { "64x64",
				 "128x128",
				 "96x96",
				 "256x256",
				 "scalable",
				 "48x48",
				 "32x32",
				 "24x24",
				 "16x16",
				 NULL };

	/* is this an absolute path */
	if (something[0] == '/') {
		_cleanup_free_ gchar *tmp;
		tmp = g_build_filename (tmpdir, something, NULL);
		if (!g_file_test (tmp, G_FILE_TEST_EXISTS)) {
			g_set_error (error,
				     ASB_PLUGIN_ERROR,
				     ASB_PLUGIN_ERROR_FAILED,
				     "specified icon '%s' does not exist",
				     something);
			return NULL;
		}
		return asb_app_load_icon (tmp, something, error);
	}

	/* hicolor apps */
	for (i = 0; sizes[i] != NULL; i++) {
		for (j = 0; supported_ext[j] != NULL; j++) {
			_cleanup_free_ gchar *log;
			_cleanup_free_ gchar *tmp;
			log = g_strdup_printf ("/usr/share/icons/hicolor/%s/apps/%s%s",
					       sizes[i],
					       something,
					       supported_ext[j]);
			tmp = g_build_filename (tmpdir, log, NULL);
			if (g_file_test (tmp, G_FILE_TEST_EXISTS))
				return asb_app_load_icon (tmp, log, error);
		}
	}

	/* pixmap */
	for (i = 0; pixmap_dirs[i] != NULL; i++) {
		for (j = 0; supported_ext[j] != NULL; j++) {
			_cleanup_free_ gchar *log;
			_cleanup_free_ gchar *tmp;
			log = g_strdup_printf ("/usr/share/%s/%s%s",
					       pixmap_dirs[i],
					       something,
					       supported_ext[j]);
			tmp = g_build_filename (tmpdir, log, NULL);
			if (g_file_test (tmp, G_FILE_TEST_EXISTS))
				return asb_app_load_icon (tmp, log, error);
		}
	}

	/* failed */
	g_set_error (error,
		     ASB_PLUGIN_ERROR,
		     ASB_PLUGIN_ERROR_FAILED,
		     "Failed to find icon %s", something);
	return NULL;
}

/**
 * asb_plugin_process_filename:
 */
static gboolean
asb_plugin_process_filename (AsbPlugin *plugin,
			     AsbPackage *pkg,
			     const gchar *filename,
			     GList **apps,
			     const gchar *tmpdir,
			     GError **error)
{
	const gchar *key;
	gboolean ret;
	_cleanup_free_ gchar *app_id = NULL;
	_cleanup_free_ gchar *full_filename = NULL;
	_cleanup_free_ gchar *icon_filename = NULL;
	_cleanup_object_unref_ AsbApp *app = NULL;
	_cleanup_object_unref_ GdkPixbuf *pixbuf = NULL;

	/* create app */
	app_id = g_path_get_basename (filename);
	app = asb_app_new (pkg, app_id);
	full_filename = g_build_filename (tmpdir, filename, NULL);
	ret = as_app_parse_file (AS_APP (app),
				 full_filename,
				 AS_APP_PARSE_FLAG_USE_HEURISTICS,
				 error);
	if (!ret)
		return FALSE;

	/* NoDisplay requires AppData */
	if (as_app_get_metadata_item (AS_APP (app), "NoDisplay") != NULL)
		asb_app_add_requires_appdata (app, "NoDisplay=true");

	/* Settings or DesktopSettings requires AppData */
	if (as_app_has_category (AS_APP (app), "Settings"))
		asb_app_add_requires_appdata (app, "Category=Settings");
	if (as_app_has_category (AS_APP (app), "DesktopSettings"))
		asb_app_add_requires_appdata (app, "Category=DesktopSettings");

	/* is the icon a stock-icon-name? */
	key = as_app_get_icon (AS_APP (app));
	if (key != NULL) {
		if (as_app_get_icon_kind (AS_APP (app)) == AS_ICON_KIND_STOCK) {
			asb_package_log (pkg,
					 ASB_PACKAGE_LOG_LEVEL_DEBUG,
					 "using stock icon %s", key);
		} else {

			/* is icon XPM or GIF */
			if (g_str_has_suffix (key, ".xpm"))
				asb_app_add_veto (app, "Uses XPM icon: %s", key);
			else if (g_str_has_suffix (key, ".gif"))
				asb_app_add_veto (app, "Uses GIF icon: %s", key);
			else if (g_str_has_suffix (key, ".ico"))
				asb_app_add_veto (app, "Uses ICO icon: %s", key);

			/* find icon */
			pixbuf = asb_app_find_icon (tmpdir, key, error);
			if (pixbuf == NULL)
				return FALSE;

			/* save in target directory */
			icon_filename = g_strdup_printf ("%s.png",
							 as_app_get_id (AS_APP (app)));
			as_app_set_icon (AS_APP (app), icon_filename, -1);
			as_app_set_icon_kind (AS_APP (app), AS_ICON_KIND_CACHED);
			asb_app_set_pixbuf (app, pixbuf);
		}
	}

	/* add */
	asb_plugin_add_app (apps, AS_APP (app));
	return TRUE;
}

/**
 * asb_plugin_process:
 */
GList *
asb_plugin_process (AsbPlugin *plugin,
		    AsbPackage *pkg,
		    const gchar *tmpdir,
		    GError **error)
{
	gboolean ret;
	GError *error_local = NULL;
	GList *apps = NULL;
	guint i;
	gchar **filelist;

	filelist = asb_package_get_filelist (pkg);
	for (i = 0; filelist[i] != NULL; i++) {
		if (!_asb_plugin_check_filename (filelist[i]))
			continue;
		ret = asb_plugin_process_filename (plugin,
						   pkg,
						   filelist[i],
						   &apps,
						   tmpdir,
						   &error_local);
		if (!ret) {
			asb_package_log (pkg,
					 ASB_PACKAGE_LOG_LEVEL_INFO,
					 "Failed to process %s: %s",
					 filelist[i],
					 error_local->message);
			g_clear_error (&error_local);
		}
	}

	/* no desktop files we care about */
	if (apps == NULL) {
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_FAILED,
			     "nothing interesting in %s",
			     asb_package_get_basename (pkg));
		return NULL;
	}
	return apps;
}
