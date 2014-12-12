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

/**
 * SECTION:asb-app
 * @short_description: Application object.
 * @stability: Unstable
 *
 * This is an application object that wraps AsApp and provides further features.
 */

#include "config.h"

#include <stdlib.h>
#include <appstream-glib.h>

#include "asb-app.h"
#include "as-cleanup.h"

typedef struct _AsbAppPrivate	AsbAppPrivate;
struct _AsbAppPrivate
{
	GPtrArray	*requires_appdata;
	AsbPackage	*pkg;
	gboolean	 ignore_requires_appdata;
	gboolean	 hidpi_enabled;
};

G_DEFINE_TYPE_WITH_PRIVATE (AsbApp, asb_app, AS_TYPE_APP)

#define GET_PRIVATE(o) (asb_app_get_instance_private (o))

/**
 * asb_app_finalize:
 **/
static void
asb_app_finalize (GObject *object)
{
	AsbApp *app = ASB_APP (object);
	AsbAppPrivate *priv = GET_PRIVATE (app);

	g_ptr_array_unref (priv->requires_appdata);
	if (priv->pkg != NULL)
		g_object_unref (priv->pkg);

	G_OBJECT_CLASS (asb_app_parent_class)->finalize (object);
}

/**
 * asb_app_init:
 **/
static void
asb_app_init (AsbApp *app)
{
	AsbAppPrivate *priv = GET_PRIVATE (app);
	priv->requires_appdata = g_ptr_array_new_with_free_func (g_free);

	/* all untrusted */
	as_app_set_trust_flags (AS_APP (app),
				AS_APP_TRUST_FLAG_CHECK_DUPLICATES |
				AS_APP_TRUST_FLAG_CHECK_VALID_UTF8);
}

/**
 * asb_app_class_init:
 **/
static void
asb_app_class_init (AsbAppClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = asb_app_finalize;
}

/**
 * asb_app_add_requires_appdata:
 * @app: A #AsbApp
 * @fmt: format string
 * @...: varargs
 *
 * Adds a reason that AppData is required.
 *
 * Since: 0.1.0
 **/
void
asb_app_add_requires_appdata (AsbApp *app, const gchar *fmt, ...)
{
	AsbAppPrivate *priv = GET_PRIVATE (app);
	gchar *tmp;
	va_list args;
	if (priv->ignore_requires_appdata)
		return;
	va_start (args, fmt);
	tmp = g_strdup_vprintf (fmt, args);
	va_end (args);
	g_ptr_array_add (priv->requires_appdata, tmp);
}

/**
 * asb_app_set_requires_appdata:
 * @app: A #AsbApp
 * @requires_appdata: boolean
 *
 * Sets (or clears) the requirement for AppData.
 *
 * Since: 0.1.0
 **/
void
asb_app_set_requires_appdata (AsbApp *app, gboolean requires_appdata)
{
	AsbAppPrivate *priv = GET_PRIVATE (app);
	if (requires_appdata) {
		if (priv->ignore_requires_appdata)
			return;
		g_ptr_array_add (priv->requires_appdata, NULL);
	} else {
		g_ptr_array_set_size (priv->requires_appdata, 0);
		priv->ignore_requires_appdata = TRUE;
	}
}

/**
 * asb_app_get_requires_appdata:
 * @app: A #AsbApp
 *
 * Gets if AppData is still required for the application.
 *
 * Returns: (transfer none) (element-type utf8): A list of reasons
 *
 * Since: 0.1.0
 **/
GPtrArray *
asb_app_get_requires_appdata (AsbApp *app)
{
	AsbAppPrivate *priv = GET_PRIVATE (app);
	return priv->requires_appdata;
}

/**
 * asb_app_get_package:
 * @app: A #AsbApp
 *
 * Gets the package that backs the application.
 *
 * Returns: (transfer none): package
 *
 * Since: 0.1.0
 **/
AsbPackage *
asb_app_get_package (AsbApp *app)
{
	AsbAppPrivate *priv = GET_PRIVATE (app);
	return priv->pkg;
}

/**
 * asb_app_save_resources_image:
 **/
static gboolean
asb_app_save_resources_image (AsbApp *app,
			      AsImage *image,
			      GError **error)
{
	const gchar *output_dir;
	gboolean ret = TRUE;
	_cleanup_free_ gchar *filename = NULL;
	_cleanup_free_ gchar *size_str = NULL;

	/* treat source images differently */
	if (as_image_get_kind (image) == AS_IMAGE_KIND_SOURCE) {
		size_str = g_strdup ("source");
	} else {
		size_str = g_strdup_printf ("%ix%i",
					    as_image_get_width (image),
					    as_image_get_height (image));
	}

	/* does screenshot already exist */
	output_dir = asb_package_get_config (asb_app_get_package (app), "ScreenshotDir");
	if (output_dir == NULL)
		return TRUE;
	filename = g_build_filename (output_dir,
				     size_str,
				     as_image_get_basename (image),
				     NULL);
	if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
		asb_package_log (asb_app_get_package (app),
				 ASB_PACKAGE_LOG_LEVEL_DEBUG,
				 "%s screenshot already exists", size_str);
		return TRUE;
	}

	/* thumbnails will already be 16:9 */
	ret = as_image_save_filename (image,
				      filename,
				      0, 0,
				      AS_IMAGE_SAVE_FLAG_NONE,
				      error);
	if (!ret)
		return FALSE;

	/* set new AppStream compatible screenshot name */
	asb_package_log (asb_app_get_package (app),
			 ASB_PACKAGE_LOG_LEVEL_DEBUG,
			 "saved %s screenshot", size_str);
	return TRUE;

}

/**
 * asb_app_save_resources_screenshot:
 **/
static gboolean
asb_app_save_resources_screenshot (AsbApp *app,
				   AsScreenshot *screenshot,
				   GError **error)
{
	AsImage *im;
	GPtrArray *images;
	guint i;

	images = as_screenshot_get_images (screenshot);
	for (i = 0; i < images->len; i++) {
		im = g_ptr_array_index (images, i);
		if (!asb_app_save_resources_image (app, im, error))
			return FALSE;

		/* clear the image data to reduce memory usage */
		as_image_set_pixbuf (im, NULL);
	}
	return TRUE;
}

/**
 * asb_app_set_hidpi_enabled:
 * @app: A #AsbApp
 * @hidpi_enabled: if HiDPI mode should be enabled
 *
 * Sets the HiDPI mode for the application.
 *
 * Since: 0.3.1
 **/
void
asb_app_set_hidpi_enabled (AsbApp *app, gboolean hidpi_enabled)
{
	AsbAppPrivate *priv = GET_PRIVATE (app);
	priv->hidpi_enabled = hidpi_enabled;
}

/**
 * asb_app_save_resources:
 * @app: A #AsbApp
 * @save_flags: #AsbAppSaveFlags, e.g. %ASB_APP_SAVE_FLAG_SCREENSHOTS
 * @error: A #GError or %NULL
 *
 * Saves to disk any resources set for the application.
 *
 * Returns: %TRUE for success, %FALSE otherwise
 *
 * Since: 0.1.0
 **/
gboolean
asb_app_save_resources (AsbApp *app, AsbAppSaveFlags save_flags, GError **error)
{
	AsbAppPrivate *priv = GET_PRIVATE (app);
	AsIcon *icon;
	AsScreenshot *ss;
	GdkPixbuf *pixbuf;
	GPtrArray *icons = NULL;
	GPtrArray *screenshots = NULL;
	guint i;

	/* any non-stock icon set */
	if (save_flags & ASB_APP_SAVE_FLAG_ICONS)
		icons = as_app_get_icons (AS_APP (app));
	for (i = 0; icons != NULL && i < icons->len; i++) {
		const gchar *tmpdir;
		_cleanup_free_ gchar *filename = NULL;
		_cleanup_free_ gchar *size_str = NULL;

		/* don't save some types of icons */
		icon = g_ptr_array_index (icons, i);
		if (as_icon_get_kind (icon) == AS_ICON_KIND_STOCK ||
		    as_icon_get_kind (icon) == AS_ICON_KIND_EMBEDDED ||
		    as_icon_get_kind (icon) == AS_ICON_KIND_LOCAL ||
		    as_icon_get_kind (icon) == AS_ICON_KIND_REMOTE)
			continue;

		/* save to disk */
		tmpdir = asb_package_get_config (priv->pkg, "TempDir");
		filename = g_build_filename (tmpdir,
					     "icons",
					     as_icon_get_name (icon),
					     NULL);
		pixbuf = as_icon_get_pixbuf (icon);
		if (pixbuf == NULL) {
			g_set_error (error,
				     AS_APP_ERROR,
				     AS_APP_ERROR_FAILED,
				     "No pixbuf for %s",
				     as_icon_get_name (icon));
			return FALSE;
		}
		if (!gdk_pixbuf_save (pixbuf, filename, "png", error, NULL))
			return FALSE;

		/* set new AppStream compatible icon name */
		asb_package_log (priv->pkg,
				 ASB_PACKAGE_LOG_LEVEL_DEBUG,
				 "Saved icon %s", filename);
	}

	/* save any screenshots */
	if (save_flags & ASB_APP_SAVE_FLAG_SCREENSHOTS)
		screenshots = as_app_get_screenshots (AS_APP (app));
	for (i = 0; screenshots != NULL && i < screenshots->len; i++) {
		ss = g_ptr_array_index (screenshots, i);
		if (!asb_app_save_resources_screenshot (app, ss, error))
			return FALSE;
	}
	return TRUE;
}

/**
 * asb_app_save_thumbnail:
 **/
static gboolean
asb_app_save_thumbnail (AsScreenshot *ss, AsImage *im_src,
			guint width, guint height, guint scale,
			const gchar *mirror_uri,
			GError **error)
{
	_cleanup_free_ gchar *size_str = NULL;
	_cleanup_free_ gchar *url_tmp = NULL;
	_cleanup_object_unref_ AsImage *im_tmp = NULL;
	_cleanup_object_unref_ GdkPixbuf *pixbuf = NULL;

	/* only save the HiDPI screenshot if it's not padded */
	if (scale > 1) {
		if (width * scale > as_image_get_width (im_src) ||
		    height * scale > as_image_get_height (im_src))
			return TRUE;
	}
	size_str = g_strdup_printf ("%ix%i", width, height);
	url_tmp = g_build_filename (mirror_uri,
				    size_str,
				    as_image_get_basename (im_src),
				    NULL);
	pixbuf = as_image_save_pixbuf (im_src,
				       width * scale,
				       height * scale,
				       AS_IMAGE_SAVE_FLAG_PAD_16_9 |
				       AS_IMAGE_SAVE_FLAG_SHARPEN);
	im_tmp = as_image_new ();
	as_image_set_width (im_tmp, width * scale);
	as_image_set_height (im_tmp, height * scale);
	as_image_set_url (im_tmp, url_tmp, -1);
	as_image_set_pixbuf (im_tmp, pixbuf);
	as_image_set_kind (im_tmp, AS_IMAGE_KIND_THUMBNAIL);
	as_image_set_basename (im_tmp, as_image_get_basename (im_src));
	as_screenshot_add_image (ss, im_tmp);
	return TRUE;
}

/**
 * asb_app_add_screenshot_source:
 * @app: A #AsbApp
 * @filename: filename to the source image
 * @error: A #GError or %NULL
 *
 * Adds a screenshot from a previously saved image.
 *
 * Returns: %TRUE for success, %FALSE otherwise
 *
 * Since: 0.1.0
 **/
gboolean
asb_app_add_screenshot_source (AsbApp *app, const gchar *filename, GError **error)
{
	AsbAppPrivate *priv = GET_PRIVATE (app);
	AsImageAlphaFlags alpha_flags;
	gboolean is_default;
	const gchar *mirror_uri;
	guint i;
	_cleanup_free_ gchar *basename = NULL;
	_cleanup_free_ gchar *filename_no_path = NULL;
	_cleanup_free_ gchar *url_src = NULL;
	_cleanup_object_unref_ AsImage *im_src = NULL;
	_cleanup_object_unref_ AsScreenshot *ss = NULL;
	guint sizes[] = { AS_IMAGE_NORMAL_WIDTH,    AS_IMAGE_NORMAL_HEIGHT,
			  AS_IMAGE_THUMBNAIL_WIDTH, AS_IMAGE_THUMBNAIL_HEIGHT,
			  AS_IMAGE_LARGE_WIDTH,     AS_IMAGE_LARGE_HEIGHT,
			  0 };

	im_src = as_image_new ();
	if (!as_image_load_filename (im_src, filename, error))
		return FALSE;

	/* is the aspect ratio of the source perfectly 16:9 */
	if ((as_image_get_width (im_src) / 16) * 9 !=
	     as_image_get_height (im_src)) {
		filename_no_path = g_path_get_basename (filename);
		asb_package_log (asb_app_get_package (app),
				 ASB_PACKAGE_LOG_LEVEL_WARNING,
				 "%s is not in 16:9 aspect ratio",
				 filename_no_path);
	}

	/* check screenshot is reasonable in size */
	if (as_image_get_width (im_src) * 2 < AS_IMAGE_NORMAL_WIDTH ||
	    as_image_get_height (im_src) * 2 < AS_IMAGE_NORMAL_HEIGHT) {
		filename_no_path = g_path_get_basename (filename);
		g_set_error (error,
			     AS_APP_ERROR,
			     AS_APP_ERROR_FAILED,
			     "%s is too small to be used: %ix%i",
			     filename_no_path,
			     as_image_get_width (im_src),
			     as_image_get_height (im_src));
		return FALSE;
	}

	/* check the image is not padded */
	alpha_flags = as_image_get_alpha_flags (im_src);
	if ((alpha_flags & AS_IMAGE_ALPHA_FLAG_TOP) > 0||
	    (alpha_flags & AS_IMAGE_ALPHA_FLAG_BOTTOM) > 0) {
		filename_no_path = g_path_get_basename (filename);
		asb_package_log (asb_app_get_package (app),
				 ASB_PACKAGE_LOG_LEVEL_WARNING,
				 "%s has vertical alpha padding",
				 filename_no_path);
	}
	if ((alpha_flags & AS_IMAGE_ALPHA_FLAG_LEFT) > 0||
	    (alpha_flags & AS_IMAGE_ALPHA_FLAG_RIGHT) > 0) {
		filename_no_path = g_path_get_basename (filename);
		asb_package_log (asb_app_get_package (app),
				 ASB_PACKAGE_LOG_LEVEL_WARNING,
				 "%s has horizontal alpha padding",
				 filename_no_path);
	}

	ss = as_screenshot_new ();
	is_default = as_app_get_screenshots(AS_APP(app))->len == 0;
	as_screenshot_set_kind (ss, is_default ? AS_SCREENSHOT_KIND_DEFAULT :
						 AS_SCREENSHOT_KIND_NORMAL);

	/* include the app-id in the basename */
	basename = g_strdup_printf ("%s-%s.png",
				    as_app_get_id_filename (AS_APP (app)),
				    as_image_get_md5 (im_src));
	as_image_set_basename (im_src, basename);

	/* fonts only have full sized screenshots */
	mirror_uri = asb_package_get_config (asb_app_get_package (app), "MirrorURI");
	url_src = g_build_filename (mirror_uri, "source", basename, NULL);
	as_image_set_url (im_src, url_src, -1);
	as_image_set_kind (im_src, AS_IMAGE_KIND_SOURCE);
	as_screenshot_add_image (ss, im_src);
	if (as_app_get_id_kind (AS_APP (app)) != AS_ID_KIND_FONT) {
		for (i = 0; sizes[i] != 0; i += 2) {

			/* save LoDPI */
			if (!asb_app_save_thumbnail (ss, im_src,
						     sizes[i], sizes[i+1],
						     1, mirror_uri, error))
				return FALSE;

			/* save HiDPI version */
			if (priv->hidpi_enabled) {
				if (!asb_app_save_thumbnail (ss, im_src,
							     sizes[i],
							     sizes[i+1],
							     2, mirror_uri,
							     error))
					return FALSE;
			}
		}
	}
	as_app_add_screenshot (AS_APP (app), ss);
	return TRUE;
}

/**
 * asb_app_new:
 * @pkg: A #AsbPackage
 * @id: The ID for the package
 *
 * Creates a new application object.
 *
 * Returns: a #AsbApp
 *
 * Since: 0.1.0
 **/
AsbApp *
asb_app_new (AsbPackage *pkg, const gchar *id)
{
	AsbApp *app;
	AsbAppPrivate *priv;

	app = g_object_new (ASB_TYPE_APP, NULL);
	priv = GET_PRIVATE (app);
	if (pkg != NULL) {
		priv->pkg = g_object_ref (pkg);
		as_app_add_pkgname (AS_APP (app),
				    asb_package_get_name (pkg), -1);
	}
	if (id != NULL)
		as_app_set_id (AS_APP (app), id, -1);
	return ASB_APP (app);
}
