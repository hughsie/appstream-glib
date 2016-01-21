/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2015 Richard Hughes <richard@hughsie.com>
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
	g_autoptr(AsImage) im = NULL;
	g_autoptr(GError) error_local = NULL;

	im = as_image_new ();
	if (!as_image_load_filename_full (im,
					  filename,
					  icon_size,
					  min_icon_size,
					  AS_IMAGE_LOAD_FLAG_NONE,
					  &error_local)) {
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_FAILED,
			     "%s: %s",
			     error_local->message, logfn);
		return NULL;
	}
	return g_object_ref (as_image_get_pixbuf (im));
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
	g_autofree gchar *fn_hidpi = NULL;
	g_autofree gchar *fn = NULL;
	g_autofree gchar *name_hidpi = NULL;
	g_autofree gchar *name = NULL;
	g_autoptr(AsIcon) icon_hidpi = NULL;
	g_autoptr(AsIcon) icon = NULL;
	g_autoptr(GdkPixbuf) pixbuf_hidpi = NULL;
	g_autoptr(GdkPixbuf) pixbuf = NULL;

	/* find 64x64 icon */
	fn = as_utils_find_icon_filename_full (tmpdir, key,
					       AS_UTILS_FIND_ICON_NONE,
					       error);
	if (fn == NULL) {
		g_prefix_error (error, "Failed to find icon: ");
		return FALSE;
	}

	/* is icon in a unsupported format */
	if (!asb_context_get_flag (plugin->ctx, ASB_CONTEXT_FLAG_IGNORE_LEGACY_ICONS)) {
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
	if (asb_context_get_flag (plugin->ctx, ASB_CONTEXT_FLAG_HIDPI_ICONS)) {
		name = g_strdup_printf ("%ix%i/%s.png",
					64, 64,
					as_app_get_id_filename (AS_APP (app)));
	} else {
		name = g_strdup_printf ("%s.png",
					as_app_get_id_filename (AS_APP (app)));
	}
	icon = as_icon_new ();
	as_icon_set_pixbuf (icon, pixbuf);
	as_icon_set_name (icon, name);
	as_icon_set_kind (icon, AS_ICON_KIND_CACHED);
	as_icon_set_prefix (icon, as_app_get_icon_path (AS_APP (app)));
	as_app_add_icon (AS_APP (app), icon);

	/* is HiDPI disabled */
	if (!asb_context_get_flag (plugin->ctx, ASB_CONTEXT_FLAG_HIDPI_ICONS))
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
	as_icon_set_name (icon_hidpi, name_hidpi);
	as_icon_set_kind (icon_hidpi, AS_ICON_KIND_CACHED);
	as_icon_set_prefix (icon_hidpi, as_app_get_icon_path (AS_APP (app)));
	as_app_add_icon (AS_APP (app), icon_hidpi);
	return TRUE;
}

/**
 * asb_plugin_desktop_refine:
 */
static gboolean
asb_plugin_desktop_refine (AsbPlugin *plugin,
			   AsbPackage *pkg,
			   const gchar *filename,
			   AsbApp *app,
			   const gchar *tmpdir,
			   GError **error)
{
	AsIcon *icon;
	AsAppParseFlags parse_flags = AS_APP_PARSE_FLAG_USE_HEURISTICS |
				      AS_APP_PARSE_FLAG_ALLOW_VETO;
	gboolean ret;
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
	as_app_subsume_full (AS_APP (app), desktop_app, AS_APP_SUBSUME_FLAG_NO_OVERWRITE);

	/* is the icon a stock-icon-name? */
	icon = as_app_get_icon_default (AS_APP (app));
	if (icon != NULL) {
		g_autofree gchar *key = NULL;
		key = g_strdup (as_icon_get_name (icon));
		if (as_icon_get_kind (icon) == AS_ICON_KIND_STOCK) {
			asb_package_log (pkg,
					 ASB_PACKAGE_LOG_LEVEL_DEBUG,
					 "using stock icon %s", key);
		} else {
			g_autoptr(GError) error_local = NULL;
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
	guint i;
	const gchar *app_dirs[] = {
		"/usr/share/applications",
		"/usr/share/applications/kde4",
		NULL };

	/* use the .desktop file to refine the application */
	for (i = 0; app_dirs[i] != NULL; i++) {
		g_autofree gchar *fn = NULL;
		fn = g_build_filename (tmpdir,
				       app_dirs[i],
				       as_app_get_id (AS_APP (app)),
				       NULL);
		if (g_file_test (fn, G_FILE_TEST_EXISTS)) {
			if (!asb_plugin_desktop_refine (plugin, pkg, fn,
							app, tmpdir, error))
				return FALSE;
		}
	}

	return TRUE;
}
