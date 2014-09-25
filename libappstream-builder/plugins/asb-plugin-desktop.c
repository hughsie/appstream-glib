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
#include <string.h>
#include <fnmatch.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <asb-plugin.h>

#define __APPSTREAM_GLIB_PRIVATE_H
#include <as-utils-private.h>
#include <as-app-private.h>

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
asb_app_load_icon (AsbPlugin *plugin,
		   AsbApp *app,
		   const gchar *filename,
		   const gchar *logfn,
		   guint icon_size,
		   GError **error)
{
	GdkPixbuf *pixbuf = NULL;
	guint min_icon_size;
	guint pixbuf_height;
	guint pixbuf_width;
	guint tmp_height;
	guint tmp_width;
	_cleanup_object_unref_ GdkPixbuf *pixbuf_src = NULL;
	_cleanup_object_unref_ GdkPixbuf *pixbuf_tmp = NULL;

	/* open file in native size */
	if (g_str_has_suffix (filename, ".svg")) {
		pixbuf_src = gdk_pixbuf_new_from_file_at_scale (filename,
								icon_size,
								icon_size,
								TRUE, error);
	} else {
		pixbuf_src = gdk_pixbuf_new_from_file (filename, error);
	}
	if (pixbuf_src == NULL)
		return NULL;

	/* check size */
	min_icon_size = asb_context_get_min_icon_size (plugin->ctx);
	if (gdk_pixbuf_get_width (pixbuf_src) < (gint) min_icon_size &&
	    gdk_pixbuf_get_height (pixbuf_src) < (gint) min_icon_size) {
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_FAILED,
			     "icon %s was too small %ix%i",
			     logfn,
			     gdk_pixbuf_get_width (pixbuf_src),
			     gdk_pixbuf_get_height (pixbuf_src));
		return NULL;
	}

	/* does the icon not have an alpha channel */
	if (!gdk_pixbuf_get_has_alpha (pixbuf_src)) {
		asb_package_log (asb_app_get_package (app),
				 ASB_PACKAGE_LOG_LEVEL_INFO,
				 "icon %s does not have an alpha channel",
				 logfn);
	}

	/* don't do anything to an icon with the perfect size */
	pixbuf_width = gdk_pixbuf_get_width (pixbuf_src);
	pixbuf_height = gdk_pixbuf_get_height (pixbuf_src);
	if (pixbuf_width == icon_size && pixbuf_height == icon_size)
		return g_object_ref (pixbuf_src);

	/* never scale up, just pad */
	if (pixbuf_width < icon_size && pixbuf_height < icon_size) {
		asb_package_log (asb_app_get_package (app),
				 ASB_PACKAGE_LOG_LEVEL_INFO,
				 "icon %s padded to %ix%i as size %ix%i",
				 logfn,
				 icon_size,
				 icon_size,
				 pixbuf_width, pixbuf_height);
		pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8,
					 icon_size, icon_size);
		gdk_pixbuf_fill (pixbuf, 0x00000000);
		gdk_pixbuf_copy_area (pixbuf_src,
				      0, 0, /* of src */
				      pixbuf_width, pixbuf_height,
				      pixbuf,
				      (icon_size - pixbuf_width) / 2,
				      (icon_size - pixbuf_height) / 2);
		return pixbuf;
	}

	/* is the aspect ratio perfectly square */
	if (pixbuf_width == pixbuf_height) {
		pixbuf = gdk_pixbuf_scale_simple (pixbuf_src,
						  icon_size, icon_size,
						  GDK_INTERP_HYPER);
		as_pixbuf_sharpen (pixbuf, 1, -0.5);
		return pixbuf;
	}

	/* create new square pixbuf with alpha padding */
	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8,
				 icon_size, icon_size);
	gdk_pixbuf_fill (pixbuf, 0x00000000);
	if (pixbuf_width > pixbuf_height) {
		tmp_width = icon_size;
		tmp_height = icon_size * pixbuf_height / pixbuf_width;
	} else {
		tmp_width = icon_size * pixbuf_width / pixbuf_height;
		tmp_height = icon_size;
	}
	pixbuf_tmp = gdk_pixbuf_scale_simple (pixbuf_src, tmp_width, tmp_height,
					      GDK_INTERP_HYPER);
	as_pixbuf_sharpen (pixbuf_tmp, 1, -0.5);
	gdk_pixbuf_copy_area (pixbuf_tmp,
			      0, 0, /* of src */
			      tmp_width, tmp_height,
			      pixbuf,
			      (icon_size - tmp_width) / 2,
			      (icon_size - tmp_height) / 2);
	return pixbuf;
}

/**
 * asb_app_find_icon:
 */
static GdkPixbuf *
asb_app_find_icon (AsbPlugin *plugin,
		   AsbApp *app,
		   const gchar *tmpdir,
		   const gchar *something,
		   GError **error)
{
	_cleanup_free_ gchar *fn = NULL;

	/* find file */
	fn = as_utils_find_icon_filename (tmpdir, something, error);
	if (fn == NULL)
		return NULL;

	/* load the icon */
	return asb_app_load_icon (plugin, app, fn, fn + strlen (tmpdir), 64, error);
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

	/* NoDisplay apps are never included */
	if (as_app_get_metadata_item (AS_APP (app), "NoDisplay") != NULL)
		as_app_add_veto (AS_APP (app), "NoDisplay=true");

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
			_cleanup_error_free_ GError *error_local = NULL;

			/* is icon XPM or GIF */
			if (g_str_has_suffix (key, ".xpm"))
				as_app_add_veto (AS_APP (app), "Uses XPM icon: %s", key);
			else if (g_str_has_suffix (key, ".gif"))
				as_app_add_veto (AS_APP (app), "Uses GIF icon: %s", key);
			else if (g_str_has_suffix (key, ".ico"))
				as_app_add_veto (AS_APP (app), "Uses ICO icon: %s", key);

			/* find icon */
			pixbuf = asb_app_find_icon (plugin, app, tmpdir,
						    key, &error_local);
			if (pixbuf == NULL) {
				as_app_add_veto (AS_APP (app), "Failed to find icon: %s",
						 error_local->message);
			} else {
				/* save in target directory */
				icon_filename = g_strdup_printf ("%s.png",
								 as_app_get_id_filename (AS_APP (app)));
				as_app_set_icon (AS_APP (app), icon_filename, -1);
				as_app_set_icon_kind (AS_APP (app), AS_ICON_KIND_CACHED);
				asb_app_add_pixbuf (app, pixbuf);
			}
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
