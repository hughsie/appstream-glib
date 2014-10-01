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
asb_app_load_icon (AsbApp *app,
		   const gchar *filename,
		   const gchar *logfn,
		   guint icon_size,
		   guint min_icon_size,
		   GError **error)
{
	GdkPixbuf *pixbuf = NULL;
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
		_cleanup_free_ gchar *size_str = NULL;
		size_str = g_strdup_printf ("%ix%i",
					    pixbuf_width,
					    pixbuf_height);
		as_app_add_metadata (AS_APP (app), "PaddedIcon",
				     size_str, -1);
		asb_package_log (asb_app_get_package (app),
				 ASB_PACKAGE_LOG_LEVEL_INFO,
				 "icon %s padded to %ix%i as size %s",
				 logfn, icon_size, icon_size, size_str);
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
 * asb_plugin_desktop_add_icons:
 */
static gboolean
asb_plugin_desktop_add_icons (AsbPlugin *plugin,
			      AsbApp *app,
			      const gchar *tmpdir,
			      const gchar *key,
			      GError **error)
{
	guint min_icon_size;
	_cleanup_free_ gchar *fn_hidpi = NULL;
	_cleanup_free_ gchar *fn = NULL;
	_cleanup_free_ gchar *name_hidpi = NULL;
	_cleanup_free_ gchar *name = NULL;
	_cleanup_object_unref_ AsIcon *icon_hidpi = NULL;
	_cleanup_object_unref_ AsIcon *icon = NULL;
	_cleanup_object_unref_ GdkPixbuf *pixbuf_hidpi = NULL;
	_cleanup_object_unref_ GdkPixbuf *pixbuf = NULL;

	/* find 64x64 icon */
	fn = as_utils_find_icon_filename_full (tmpdir, key,
					       AS_UTILS_FIND_ICON_NONE,
					       error);
	if (fn == NULL) {
		g_prefix_error (error, "Failed to find icon: ");
		return FALSE;
	}

	/* is icon in a unsupported format */
	if (g_str_has_suffix (fn, ".xpm")) {
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_NOT_SUPPORTED,
			     "Uses XPM icon: %s", key);
		return FALSE;
	}
	if (g_str_has_suffix (fn, ".gif")) {
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_NOT_SUPPORTED,
			     "Uses GIF icon: %s", key);
		return FALSE;
	}
	if (g_str_has_suffix (fn, ".ico")) {
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_NOT_SUPPORTED,
			     "Uses ICO icon: %s", key);
		return FALSE;
	}

	/* load the icon */
	min_icon_size = asb_context_get_min_icon_size (plugin->ctx);
	pixbuf = asb_app_load_icon (app, fn, fn + strlen (tmpdir),
				    64, min_icon_size, error);
	if (pixbuf == NULL) {
		g_prefix_error (error, "Failed to load icon: ");
		return FALSE;
	}

	/* save in target directory */
	if (asb_context_get_hidpi_enabled (plugin->ctx)) {
		name = g_strdup_printf ("%ix%i/%s.png",
					64, 64,
					as_app_get_id_filename (AS_APP (app)));
	} else {
		name = g_strdup_printf ("%s.png",
					as_app_get_id_filename (AS_APP (app)));
	}
	icon = as_icon_new ();
	as_icon_set_pixbuf (icon, pixbuf);
	as_icon_set_name (icon, name, -1);
	as_icon_set_kind (icon, AS_ICON_KIND_CACHED);
	as_icon_set_prefix (icon, as_app_get_icon_path (AS_APP (app)));
	as_app_add_icon (AS_APP (app), icon);

	/* is HiDPI disabled */
	if (!asb_context_get_hidpi_enabled (plugin->ctx))
		return TRUE;

	/* try to get a HiDPI icon */
	fn_hidpi = as_utils_find_icon_filename_full (tmpdir, key,
						     AS_UTILS_FIND_ICON_HI_DPI,
						     NULL);
	if (fn_hidpi == NULL)
		return TRUE;

	/* load the HiDPI icon */
	pixbuf_hidpi = asb_app_load_icon (app, fn_hidpi,
					  fn_hidpi + strlen (tmpdir),
					  128, 128, NULL);
	if (pixbuf_hidpi == NULL)
		return TRUE;
	if (gdk_pixbuf_get_width (pixbuf_hidpi) <= gdk_pixbuf_get_width (pixbuf) ||
	    gdk_pixbuf_get_height (pixbuf_hidpi) <= gdk_pixbuf_get_height (pixbuf))
		return TRUE;
	as_app_add_kudo_kind (AS_APP (app), AS_KUDO_KIND_HI_DPI_ICON);

	/* save icon */
	name_hidpi = g_strdup_printf ("%ix%i/%s.png",
				      128, 128,
				      as_app_get_id_filename (AS_APP (app)));
	icon_hidpi = as_icon_new ();
	as_icon_set_pixbuf (icon_hidpi, pixbuf_hidpi);
	as_icon_set_name (icon_hidpi, name_hidpi, -1);
	as_icon_set_kind (icon_hidpi, AS_ICON_KIND_CACHED);
	as_icon_set_prefix (icon_hidpi, as_app_get_icon_path (AS_APP (app)));
	as_app_add_icon (AS_APP (app), icon_hidpi);
	return TRUE;
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
	AsIcon *icon;
	gboolean ret;
	_cleanup_free_ gchar *app_id = NULL;
	_cleanup_free_ gchar *full_filename = NULL;
	_cleanup_object_unref_ AsbApp *app = NULL;
	_cleanup_object_unref_ GdkPixbuf *pixbuf = NULL;

	/* create app */
	app_id = g_path_get_basename (filename);
	app = asb_app_new (pkg, app_id);
	asb_app_set_hidpi_enabled (app, asb_context_get_hidpi_enabled (plugin->ctx));
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
	icon = as_app_get_icon_default (AS_APP (app));
	if (icon != NULL) {
		_cleanup_free_ gchar *key;
		key = g_strdup (as_icon_get_name (icon));
		if (as_icon_get_kind (icon) == AS_ICON_KIND_STOCK) {
			asb_package_log (pkg,
					 ASB_PACKAGE_LOG_LEVEL_DEBUG,
					 "using stock icon %s", key);
		} else {
			_cleanup_error_free_ GError *error_local = NULL;
			g_ptr_array_set_size (as_app_get_icons (AS_APP (app)), 0);
			ret = asb_plugin_desktop_add_icons (plugin,
							    app,
							    tmpdir,
							    key,
							    &error_local);
			if (!ret) {
				as_app_add_veto (AS_APP (app), "%s",
						 error_local->message);
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
