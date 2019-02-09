/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include <config.h>
#include <string.h>
#include <fnmatch.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <asb-plugin.h>

#define __APPSTREAM_GLIB_PRIVATE_H
#include <as-utils-private.h>
#include <as-app-private.h>

const gchar *
asb_plugin_get_name (void)
{
	return "icon";
}

void
asb_plugin_add_globs (AsbPlugin *plugin, GPtrArray *globs)
{
	asb_plugin_add_glob (globs, "/usr/share/pixmaps/*");
	asb_plugin_add_glob (globs, "/usr/share/icons/*");
	asb_plugin_add_glob (globs, "/usr/share/*/icons/*");
}

static GdkPixbuf *
asb_app_load_icon (AsbPlugin *plugin,
		   const gchar *filename,
		   const gchar *logfn,
		   guint icon_size,
		   guint min_icon_size,
		   GError **error)
{
	g_autoptr(AsImage) im = NULL;
	g_autoptr(GError) error_local = NULL;
	AsImageLoadFlags load_flags = AS_IMAGE_LOAD_FLAG_ALWAYS_RESIZE;

	/* is icon in a unsupported format */
	if (!asb_context_get_flag (plugin->ctx, ASB_CONTEXT_FLAG_IGNORE_LEGACY_ICONS))
		load_flags |= AS_IMAGE_LOAD_FLAG_ONLY_SUPPORTED;

	im = as_image_new ();
	if (!as_image_load_filename_full (im,
					  filename,
					  icon_size,
					  min_icon_size,
					  load_flags,
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

static gboolean
asb_plugin_icon_convert_cached (AsbPlugin *plugin,
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

	/* load the icon */
	min_icon_size = asb_context_get_min_icon_size (plugin->ctx);
	pixbuf = asb_app_load_icon (plugin, fn, fn + strlen (tmpdir),
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
	pixbuf_hidpi = asb_app_load_icon (plugin, fn_hidpi,
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

gboolean
asb_plugin_process_app (AsbPlugin *plugin,
			AsbPackage *pkg,
			AsbApp *app,
			const gchar *tmpdir,
			GError **error)
{
	AsIcon *icon;
	g_autofree gchar *key = NULL;
	g_autoptr(GError) error_local = NULL;

	/* no icon defined */
	icon = as_app_get_icon_default (AS_APP (app));
	if (icon == NULL)
		return TRUE;

	/* is the icon a stock-icon-name? */
	if (as_icon_get_kind (icon) == AS_ICON_KIND_STOCK) {
		asb_package_log (pkg,
				 ASB_PACKAGE_LOG_LEVEL_DEBUG,
				 "using stock icon %s",
				 as_icon_get_name (icon));
		return TRUE;
	}

	/* already a cached icon, e.g. a font */
	if (as_icon_get_kind (icon) == AS_ICON_KIND_CACHED)
		return TRUE;

	/* convert to cached */
	switch (as_icon_get_kind (icon)) {
	case AS_ICON_KIND_LOCAL:
		key = g_strdup (as_icon_get_filename (icon));
		break;
	default:
		key = g_strdup (as_icon_get_name (icon));
		break;
	}
	g_ptr_array_set_size (as_app_get_icons (AS_APP (app)), 0);
	if (!asb_plugin_icon_convert_cached (plugin, app, tmpdir, key, &error_local))
		as_app_add_veto (AS_APP (app), "%s", error_local->message);

	return TRUE;
}
