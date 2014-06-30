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
	GPtrArray	*vetos;
	GPtrArray	*requires_appdata;
	GdkPixbuf	*pixbuf;
	AsbPackage	*pkg;
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

	g_ptr_array_unref (priv->vetos);
	g_ptr_array_unref (priv->requires_appdata);
	if (priv->pixbuf != NULL)
		g_object_unref (priv->pixbuf);
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
	priv->vetos = g_ptr_array_new_with_free_func (g_free);
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
 * asb_app_to_xml:
 * @app: A #AsbApp
 *
 * Converts the application to it's XML representation.
 *
 * Returns: allocated string
 *
 * Since: 0.1.0
 **/
gchar *
asb_app_to_xml (AsbApp *app)
{
	GString *str;
	_cleanup_object_unref_ AsStore *store;

	store = as_store_new ();
	as_store_set_api_version (store, 1.0f);
	as_store_add_app (store, AS_APP (app));
	str = as_store_to_xml (store,
			       AS_NODE_TO_XML_FLAG_FORMAT_INDENT |
			       AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE);
	return g_string_free (str, FALSE);
}

/**
 * asb_app_add_veto:
 * @app: A #AsbApp
 * @fmt: format string
 * @...: varargs
 *
 * Adds a reason to not include the application.
 *
 * Since: 0.1.0
 **/
void
asb_app_add_veto (AsbApp *app, const gchar *fmt, ...)
{
	AsbAppPrivate *priv = GET_PRIVATE (app);
	gchar *tmp;
	va_list args;
	va_start (args, fmt);
	tmp = g_strdup_vprintf (fmt, args);
	va_end (args);
	g_ptr_array_add (priv->vetos, tmp);
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
		g_ptr_array_add (priv->requires_appdata, NULL);
	} else {
		g_ptr_array_set_size (priv->requires_appdata, 0);
	}
}

/**
 * asb_app_set_pixbuf:
 * @app: A #AsbApp
 * @pixbuf: a #GdkPixbuf
 *
 * Sets the icon for the application.
 *
 * Since: 0.1.0
 **/
void
asb_app_set_pixbuf (AsbApp *app, GdkPixbuf *pixbuf)
{
	AsbAppPrivate *priv = GET_PRIVATE (app);
	if (priv->pixbuf != NULL)
		g_object_ref (priv->pixbuf);
	priv->pixbuf = g_object_ref (pixbuf);

	/* does the icon not have an alpha channel */
	if (!gdk_pixbuf_get_has_alpha (priv->pixbuf)) {
		asb_package_log (priv->pkg,
				 ASB_PACKAGE_LOG_LEVEL_WARNING,
				 "icon does not have an alpha channel");
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
 * asb_app_get_vetos:
 * @app: A #AsbApp
 *
 * Gets the list of vetos.
 *
 * Returns: (transfer none) (element-type utf8): A list of vetos
 *
 * Since: 0.1.0
 **/
GPtrArray *
asb_app_get_vetos (AsbApp *app)
{
	AsbAppPrivate *priv = GET_PRIVATE (app);
	return priv->vetos;
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
	_cleanup_free_ gchar *size_str;

	/* treat source images differently */
	if (as_image_get_kind (image) == AS_IMAGE_KIND_SOURCE) {
		size_str = g_strdup ("source");
	} else {
		size_str = g_strdup_printf ("%ix%i",
					    as_image_get_width (image),
					    as_image_get_height (image));
	}

	/* does screenshot already exist */
	output_dir = asb_package_get_config (asb_app_get_package (app), "OutputDir");
	filename = g_build_filename (output_dir,
				     "screenshots",
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
	}
	return TRUE;
}

/**
 * asb_app_save_resources:
 * @app: A #AsbApp
 * @error: A #GError or %NULL
 *
 * Saves to disk any resources set for the application.
 *
 * Returns: %TRUE for success, %FALSE otherwise
 *
 * Since: 0.1.0
 **/
gboolean
asb_app_save_resources (AsbApp *app, GError **error)
{
	AsbAppPrivate *priv = GET_PRIVATE (app);
	AsScreenshot *ss;
	guint i;
	GPtrArray *screenshots;

	/* any non-stock icon set */
	if (priv->pixbuf != NULL) {
		const gchar *tmpdir;
		_cleanup_free_ gchar *filename = NULL;

		tmpdir = asb_package_get_config (priv->pkg, "TempDir");
		filename = g_build_filename (tmpdir,
					     "icons",
					     as_app_get_icon (AS_APP (app)),
					     NULL);
		if (!gdk_pixbuf_save (priv->pixbuf, filename, "png", error, NULL))
			return FALSE;

		/* set new AppStream compatible icon name */
		asb_package_log (priv->pkg,
				 ASB_PACKAGE_LOG_LEVEL_DEBUG,
				 "Saved icon %s", filename);
	}

	/* save any screenshots */
	screenshots = as_app_get_screenshots (AS_APP (app));
	for (i = 0; i < screenshots->len; i++) {
		ss = g_ptr_array_index (screenshots, i);
		if (!asb_app_save_resources_screenshot (app, ss, error))
			return FALSE;
	}
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
	gboolean is_default;
	guint sizes[] = { 624, 351, 112, 63, 752, 423, 0 };
	const gchar *mirror_uri;
	guint i;
	_cleanup_free_ gchar *basename = NULL;
	_cleanup_object_unref_ AsImage *im_src;
	_cleanup_object_unref_ AsScreenshot *ss = NULL;

	im_src = as_image_new ();
	if (!as_image_load_filename (im_src, filename, error))
		return FALSE;

	/* is the aspect ratio of the source perfectly 16:9 */
	if ((as_image_get_width (im_src) / 16) * 9 !=
	     as_image_get_height (im_src)) {
		asb_package_log (asb_app_get_package (app),
				 ASB_PACKAGE_LOG_LEVEL_WARNING,
				 "%s is not in 16:9 aspect ratio",
				 filename);
	}

	ss = as_screenshot_new ();
	is_default = as_app_get_screenshots(AS_APP(app))->len == 0;
	as_screenshot_set_kind (ss, is_default ? AS_SCREENSHOT_KIND_DEFAULT :
						 AS_SCREENSHOT_KIND_NORMAL);

	/* include the app-id in the basename */
	basename = g_strdup_printf ("%s-%s.png",
				    as_app_get_id (AS_APP (app)),
				    as_image_get_md5 (im_src));
	as_image_set_basename (im_src, basename);

	/* only fonts have full sized screenshots */
	mirror_uri = asb_package_get_config (asb_app_get_package (app), "MirrorURI");
	if (as_app_get_id_kind (AS_APP (app)) == AS_ID_KIND_FONT) {
		_cleanup_free_ gchar *url_tmp;
		url_tmp = g_build_filename (mirror_uri,
					    "source",
					    basename,
					    NULL);
		as_image_set_url (im_src, url_tmp, -1);
		as_image_set_kind (im_src, AS_IMAGE_KIND_SOURCE);
		as_screenshot_add_image (ss, im_src);
	} else {
		for (i = 0; sizes[i] != 0; i += 2) {
			_cleanup_free_ gchar *size_str;
			_cleanup_free_ gchar *url_tmp;
			_cleanup_object_unref_ AsImage *im_tmp;
			_cleanup_object_unref_ GdkPixbuf *pixbuf;

			size_str = g_strdup_printf ("%ix%i",
						    sizes[i],
						    sizes[i+1]);
			url_tmp = g_build_filename (mirror_uri,
						    size_str,
						    basename,
						    NULL);
			pixbuf = as_image_save_pixbuf (im_src,
						       sizes[i],
						       sizes[i+1],
						       AS_IMAGE_SAVE_FLAG_PAD_16_9);
			im_tmp = as_image_new ();
			as_image_set_width (im_tmp, sizes[i]);
			as_image_set_height (im_tmp, sizes[i+1]);
			as_image_set_url (im_tmp, url_tmp, -1);
			as_image_set_pixbuf (im_tmp, pixbuf);
			as_image_set_kind (im_tmp, AS_IMAGE_KIND_THUMBNAIL);
			as_image_set_basename (im_tmp, basename);
			as_screenshot_add_image (ss, im_tmp);
		}
	}
	as_app_add_screenshot (AS_APP (app), ss);
	return TRUE;
}

/**
 * asb_app_new:
 * @pkg: A #AsbPackage
 * @id_full: The ID for the package
 *
 * Creates a new application object.
 *
 * Returns: a #AsbApp
 *
 * Since: 0.1.0
 **/
AsbApp *
asb_app_new (AsbPackage *pkg, const gchar *id_full)
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
	if (id_full != NULL)
		as_app_set_id_full (AS_APP (app), id_full, -1);
	return ASB_APP (app);
}
