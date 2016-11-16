/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2016 Richard Hughes <richard@hughsie.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

/**
 * SECTION:as-screenshot
 * @short_description: Object representing a single screenshot
 * @include: appstream-glib.h
 * @stability: Stable
 *
 * Screenshots have a localized caption and also contain a number of images
 * of different resolution.
 *
 * See also: #AsImage
 */

#include "config.h"

#include "as-image-private.h"
#include "as-node-private.h"
#include "as-ref-string.h"
#include "as-screenshot-private.h"
#include "as-tag.h"
#include "as-utils-private.h"
#include "as-yaml.h"

typedef struct
{
	AsScreenshotKind	 kind;
	GHashTable		*captions;
	GPtrArray		*images;
	gint			 priority;
} AsScreenshotPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsScreenshot, as_screenshot, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_screenshot_get_instance_private (o))

static void
as_screenshot_finalize (GObject *object)
{
	AsScreenshot *screenshot = AS_SCREENSHOT (object);
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);

	g_ptr_array_unref (priv->images);
	g_hash_table_unref (priv->captions);

	G_OBJECT_CLASS (as_screenshot_parent_class)->finalize (object);
}

static void
as_screenshot_init (AsScreenshot *screenshot)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	priv->kind = AS_SCREENSHOT_KIND_NORMAL;
	priv->images = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->captions = g_hash_table_new_full (g_str_hash,
						g_str_equal,
						(GDestroyNotify) as_ref_string_unref,
						(GDestroyNotify) as_ref_string_unref);
}

static void
as_screenshot_class_init (AsScreenshotClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_screenshot_finalize;
}

/**
 * as_screenshot_kind_from_string:
 * @kind: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: (transfer full): a %AsScreenshotKind, or
 *                           %AS_SCREENSHOT_KIND_UNKNOWN if not known.
 *
 * Since: 0.1.0
 **/
AsScreenshotKind
as_screenshot_kind_from_string (const gchar *kind)
{
	if (g_strcmp0 (kind, "normal") == 0)
		return AS_SCREENSHOT_KIND_NORMAL;
	if (g_strcmp0 (kind, "default") == 0)
		return AS_SCREENSHOT_KIND_DEFAULT;
	return AS_SCREENSHOT_KIND_UNKNOWN;
}

/**
 * as_screenshot_kind_to_string:
 * @kind: the #AsScreenshotKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @kind
 *
 * Since: 0.1.0
 **/
const gchar *
as_screenshot_kind_to_string (AsScreenshotKind kind)
{
	if (kind == AS_SCREENSHOT_KIND_NORMAL)
		return "normal";
	if (kind == AS_SCREENSHOT_KIND_DEFAULT)
		return "default";
	return NULL;
}

/**
 * as_screenshot_get_kind:
 * @screenshot: a #AsScreenshot instance.
 *
 * Gets the screenshot kind.
 *
 * Returns: a #AsScreenshotKind
 *
 * Since: 0.1.0
 **/
AsScreenshotKind
as_screenshot_get_kind (AsScreenshot *screenshot)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	return priv->kind;
}

/**
 * as_screenshot_get_priority:
 * @screenshot: a #AsScreenshot instance.
 *
 * Gets the screenshot priority.
 *
 * Returns: a priority value
 *
 * Since: 0.3.1
 **/
gint
as_screenshot_get_priority (AsScreenshot *screenshot)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	return priv->priority;
}

/**
 * as_screenshot_get_images:
 * @screenshot: a #AsScreenshot instance.
 *
 * Gets the images included in the screenshot of all sizes and locales.
 *
 * Returns: (element-type AsImage) (transfer none): an array
 *
 * Since: 0.1.0
 **/
GPtrArray *
as_screenshot_get_images (AsScreenshot *screenshot)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	return priv->images;
}

/**
 * as_screenshot_get_images_for_locale:
 * @screenshot: a #AsScreenshot instance.
 * @locale: a locale, e.g. `en_GB`
 *
 * Returns all images of all sizes that are compatible with a specific locale.
 *
 * Returns: (element-type AsImage) (transfer container): an array
 *
 * Since: 0.5.14
 **/
GPtrArray *
as_screenshot_get_images_for_locale (AsScreenshot *screenshot,
				    const gchar *locale)
{
	AsImage *im;
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	GPtrArray *array;
	guint i;

	/* user wants a specific locale */
	array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	for (i = 0; i < priv->images->len; i++) {
		im = g_ptr_array_index (priv->images, i);
		if (!as_utils_locale_is_compatible (as_image_get_locale (im),
						    locale))
			continue;
		g_ptr_array_add (array, g_object_ref (im));
	}
	return array;
}

/**
 * as_screenshot_get_image_for_locale:
 * @screenshot: a #AsScreenshot instance.
 * @locale: locale, or %NULL
 * @width: target width
 * @height: target height
 *
 * Gets the AsImage closest to the target size with the specified locale.
 * The #AsImage may not actually be the requested size, and the application may
 * have to pad / rescale the image to make it fit.
 *
 * FIXME: This function assumes the images are ordered in preference order, e.g.
 * "en_GB -> en -> NULL"
 *
 * Returns: (transfer none): an #AsImage, or %NULL
 *
 * Since: 0.5.14
 **/
AsImage *
as_screenshot_get_image_for_locale (AsScreenshot *screenshot,
				    const gchar *locale,
				    guint width, guint height)
{
	AsImage *im;
	AsImage *im_best = NULL;
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	gint64 best_size = G_MAXINT64;
	guint i;
	gint64 tmp;

	g_return_val_if_fail (AS_IS_SCREENSHOT (screenshot), NULL);

	for (i = 0; i < priv->images->len; i++) {
		im = g_ptr_array_index (priv->images, i);
		if (!as_utils_locale_is_compatible (as_image_get_locale (im), locale))
			continue;
		tmp = ABS ((gint64) (width * height) -
			   (gint64) (as_image_get_width (im) * as_image_get_height (im)));
		if (tmp < best_size) {
			best_size = tmp;
			im_best = im;
		}
	}
	return im_best;
}

/**
 * as_screenshot_get_image:
 * @screenshot: a #AsScreenshot instance.
 * @width: target width
 * @height: target height
 *
 * Gets the AsImage closest to the target size. The #AsImage may not actually
 * be the requested size, and the application may have to pad / rescale the
 * image to make it fit.
 *
 * Returns: (transfer none): an #AsImage, or %NULL
 *
 * Since: 0.2.2
 **/
AsImage *
as_screenshot_get_image (AsScreenshot *screenshot, guint width, guint height)
{
	return as_screenshot_get_image_for_locale (screenshot,
						   NULL, /* locale */
						   width,
						   height);
}

/**
 * as_screenshot_get_source:
 * @screenshot: a #AsScreenshot instance.
 *
 * Gets the source image (the unscaled version) for the screenshot
 *
 * Returns: (transfer none): an #AsImage, or %NULL
 *
 * Since: 0.1.6
 **/
AsImage *
as_screenshot_get_source (AsScreenshot *screenshot)
{
	AsImage *im;
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	guint i;

	for (i = 0; i < priv->images->len; i++) {
		im = g_ptr_array_index (priv->images, i);
		if (as_image_get_kind (im) == AS_IMAGE_KIND_SOURCE)
			return im;
	}
	return NULL;
}

/**
 * as_screenshot_get_caption:
 * @screenshot: a #AsScreenshot instance.
 * @locale: the locale, or %NULL. e.g. "en_GB"
 *
 * Gets the image caption for a specific locale.
 *
 * Returns: the caption
 *
 * Since: 0.1.0
 **/
const gchar *
as_screenshot_get_caption (AsScreenshot *screenshot, const gchar *locale)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	return as_hash_lookup_by_locale (priv->captions, locale);
}

/**
 * as_screenshot_set_priority:
 * @screenshot: a #AsScreenshot instance.
 * @priority: the priority value.
 *
 * Sets the screenshot priority. Higher numbers are better.
 *
 * Since: 0.3.1
 **/
void
as_screenshot_set_priority (AsScreenshot *screenshot, gint priority)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	priv->priority = priority;
}

/**
 * as_screenshot_set_kind:
 * @screenshot: a #AsScreenshot instance.
 * @kind: the #AsScreenshotKind.
 *
 * Sets the screenshot kind.
 *
 * Since: 0.1.0
 **/
void
as_screenshot_set_kind (AsScreenshot *screenshot, AsScreenshotKind kind)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	priv->kind = kind;
}

/**
 * as_screenshot_add_image:
 * @screenshot: a #AsScreenshot instance.
 * @image: a #AsImage instance.
 *
 * Adds an image to the screenshot.
 *
 * Since: 0.1.0
 **/
void
as_screenshot_add_image (AsScreenshot *screenshot, AsImage *image)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	g_ptr_array_add (priv->images, g_object_ref (image));
}

/**
 * as_screenshot_set_caption:
 * @screenshot: a #AsScreenshot instance.
 * @locale: the locale, or %NULL. e.g. "en_GB"
 * @caption: the caption text.
 *
 * Sets a caption on the screenshot for a specific locale.
 *
 * Since: 0.1.0
 **/
void
as_screenshot_set_caption (AsScreenshot *screenshot,
			   const gchar *locale,
			   const gchar *caption)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	if (locale == NULL)
		locale = "C";
	g_hash_table_insert (priv->captions,
			     as_ref_string_new (locale),
			     as_ref_string_new (caption));
}

/**
 * as_screenshot_node_insert: (skip)
 * @screenshot: a #AsScreenshot instance.
 * @parent: the parent #GNode to use..
 * @ctx: the #AsNodeContext
 *
 * Inserts the screenshot into the DOM tree.
 *
 * Returns: (transfer none): A populated #GNode, or %NULL
 *
 * Since: 0.1.1
 **/
GNode *
as_screenshot_node_insert (AsScreenshot *screenshot,
			   GNode *parent,
			   AsNodeContext *ctx)
{
	AsImage *image;
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	GNode *n;
	guint i;

	/* nothing to add */
	if (priv->images->len == 0)
		return NULL;

	n = as_node_insert (parent, "screenshot", NULL,
			    AS_NODE_INSERT_FLAG_NONE,
			    NULL);
	if (priv->kind != AS_SCREENSHOT_KIND_NORMAL) {
		as_node_add_attribute (n, "type",
				       as_screenshot_kind_to_string (priv->kind));
	}
	if (as_node_context_get_version (ctx) >= 0.41) {
		as_node_insert_localized (n,
					  "caption",
					  priv->captions,
					  AS_NODE_INSERT_FLAG_DEDUPE_LANG);
	}
	if (as_node_context_get_version (ctx) >= 0.8 && priv->priority != 0)
		as_node_add_attribute_as_int (n, "priority", priv->priority);
	for (i = 0; i < priv->images->len; i++) {
		image = g_ptr_array_index (priv->images, i);
		as_image_node_insert (image, n, ctx);
	}
	return n;
}

/**
 * as_screenshot_node_parse:
 * @screenshot: a #AsScreenshot instance.
 * @node: a #GNode.
 * @ctx: a #AsNodeContext.
 * @error: A #GError or %NULL.
 *
 * Populates the object from a DOM node.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.1.0
 **/
gboolean
as_screenshot_node_parse (AsScreenshot *screenshot, GNode *node,
			  AsNodeContext *ctx, GError **error)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	GList *l;
	GNode *c;
	const gchar *tmp;
	guint size;
	gint priority;
	g_autoptr(GHashTable) captions = NULL;

	tmp = as_node_get_attribute (node, "type");
	if (tmp != NULL) {
		as_screenshot_set_kind (screenshot,
					as_screenshot_kind_from_string (tmp));
	}
	priority = as_node_get_attribute_as_int (node, "priority");
	if (priority != G_MAXINT)
		as_screenshot_set_priority (screenshot, priority);

	/* add captions */
	captions = as_node_get_localized (node, "caption");
	if (captions != NULL) {
		g_autoptr(GList) keys = NULL;
		keys = g_hash_table_get_keys (captions);
		for (l = keys; l != NULL; l = l->next) {
			tmp = l->data;
			as_screenshot_set_caption (screenshot,
						   tmp,
						   g_hash_table_lookup (captions, tmp));
		}
	}

	/* AppData files does not have <image> tags */
	tmp = as_node_get_data (node);
	if (tmp != NULL) {
		AsImage *image;
		image = as_image_new ();
		as_image_set_kind (image, AS_IMAGE_KIND_SOURCE);
		size = as_node_get_attribute_as_uint (node, "width");
		if (size != G_MAXUINT)
			as_image_set_width (image, size);
		size = as_node_get_attribute_as_uint (node, "height");
		if (size != G_MAXUINT)
			as_image_set_height (image, size);
		as_image_set_url (image, tmp);
		g_ptr_array_add (priv->images, image);
	}

	/* add images */
	for (c = node->children; c != NULL; c = c->next) {
		g_autoptr(AsImage) image = NULL;
		if (as_node_get_tag (c) != AS_TAG_IMAGE)
			continue;
		image = as_image_new ();
		if (!as_image_node_parse (image, c, ctx, error))
			return FALSE;
		g_ptr_array_add (priv->images, g_object_ref (image));
	}
	return TRUE;
}

/**
 * as_screenshot_node_parse_dep11:
 * @screenshot: a #AsScreenshot instance.
 * @node: a #GNode.
 * @ctx: a #AsNodeContext.
 * @error: A #GError or %NULL.
 *
 * Populates the object from a DEP-11 node.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.3.0
 **/
gboolean
as_screenshot_node_parse_dep11 (AsScreenshot *ss, GNode *node,
				AsNodeContext *ctx, GError **error)
{
	GNode *c;
	GNode *n;
	const gchar *tmp;

	for (n = node->children; n != NULL; n = n->next) {
		tmp = as_yaml_node_get_key (n);
		if (g_strcmp0 (tmp, "default") == 0) {
			if (g_strcmp0 (as_yaml_node_get_value (n), "true") == 0)
				as_screenshot_set_kind (ss, AS_SCREENSHOT_KIND_DEFAULT);
			else if (g_strcmp0 (as_yaml_node_get_value (n), "false") == 0)
				as_screenshot_set_kind (ss, AS_SCREENSHOT_KIND_NORMAL);
			continue;
		}
		if (g_strcmp0 (tmp, "source-image") == 0) {
			g_autoptr(AsImage) im = as_image_new ();
			as_image_set_kind (im, AS_IMAGE_KIND_SOURCE);
			if (!as_image_node_parse_dep11 (im, n, ctx, error))
				return FALSE;
			as_screenshot_add_image (ss, im);
			continue;
		}
		if (g_strcmp0 (tmp, "thumbnails") == 0) {
			for (c = n->children; c != NULL; c = c->next) {
				g_autoptr(AsImage) im = as_image_new ();
				as_image_set_kind (im, AS_IMAGE_KIND_THUMBNAIL);
				if (!as_image_node_parse_dep11 (im, c, ctx, error))
					return FALSE;
				as_screenshot_add_image (ss, im);
			}
			continue;
		}
	}
	return TRUE;
}

/**
 * as_screenshot_equal:
 * @screenshot1: a #AsScreenshot instance.
 * @screenshot2: a #AsScreenshot instance.
 *
 * Checks if two screenshots are the same.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.5.7
 **/
gboolean
as_screenshot_equal (AsScreenshot *screenshot1, AsScreenshot *screenshot2)
{
	AsScreenshotPrivate *priv1 = GET_PRIVATE (screenshot1);
	AsScreenshotPrivate *priv2 = GET_PRIVATE (screenshot2);
	AsImage *im1;
	AsImage *im2;

	/* trivial */
	if (screenshot1 == screenshot2)
		return TRUE;

	/* check for equality */
	if (priv1->priority != priv2->priority)
		return FALSE;
	if (priv1->images->len != priv2->images->len)
		return FALSE;
	if (g_strcmp0 (as_screenshot_get_caption (screenshot1, NULL),
		       as_screenshot_get_caption (screenshot2, NULL)) != 0)
		return FALSE;

	/* check source images */
	im1 = as_screenshot_get_source (screenshot1);
	im2 = as_screenshot_get_source (screenshot2);
	if (im1 != NULL && im2 != NULL) {
		if (!as_image_equal (im1, im2))
			return FALSE;
	}

	/* success */
	return TRUE;
}

/**
 * as_screenshot_new:
 *
 * Creates a new #AsScreenshot.
 *
 * Returns: (transfer full): a #AsScreenshot
 *
 * Since: 0.1.0
 **/
AsScreenshot *
as_screenshot_new (void)
{
	AsScreenshot *screenshot;
	screenshot = g_object_new (AS_TYPE_SCREENSHOT, NULL);
	return AS_SCREENSHOT (screenshot);
}
