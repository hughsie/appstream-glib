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

#include "as-cleanup.h"
#include "as-image-private.h"
#include "as-node-private.h"
#include "as-screenshot-private.h"
#include "as-tag.h"
#include "as-utils-private.h"

typedef struct _AsScreenshotPrivate	AsScreenshotPrivate;
struct _AsScreenshotPrivate
{
	AsScreenshotKind	 kind;
	GHashTable		*captions;
	GPtrArray		*images;
};

G_DEFINE_TYPE_WITH_PRIVATE (AsScreenshot, as_screenshot, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_screenshot_get_instance_private (o))

/**
 * as_screenshot_finalize:
 **/
static void
as_screenshot_finalize (GObject *object)
{
	AsScreenshot *screenshot = AS_SCREENSHOT (object);
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);

	g_ptr_array_unref (priv->images);
	g_hash_table_unref (priv->captions);

	G_OBJECT_CLASS (as_screenshot_parent_class)->finalize (object);
}

/**
 * as_screenshot_init:
 **/
static void
as_screenshot_init (AsScreenshot *screenshot)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	priv->kind = AS_SCREENSHOT_KIND_NORMAL;
	priv->images = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->captions = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
}

/**
 * as_screenshot_class_init:
 **/
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
 * as_screenshot_get_images:
 * @screenshot: a #AsScreenshot instance.
 *
 * Gets the image sizes included in the screenshot.
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
 * @caption_len: the size of @caption, or -1 if %NULL-terminated.
 *
 * Sets a caption on the screenshot for a specific locale.
 *
 * Since: 0.1.0
 **/
void
as_screenshot_set_caption (AsScreenshot *screenshot,
			   const gchar *locale,
			   const gchar *caption,
			   gsize caption_len)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	if (locale == NULL)
		locale = "C";
	g_hash_table_insert (priv->captions,
			     g_strdup (locale),
			     as_strndup (caption, caption_len));
}

/**
 * as_screenshot_node_insert: (skip)
 * @screenshot: a #AsScreenshot instance.
 * @parent: the parent #GNode to use..
 * @api_version: the AppStream API version
 *
 * Inserts the screenshot into the DOM tree.
 *
 * Returns: (transfer full): A populated #GNode
 *
 * Since: 0.1.1
 **/
GNode *
as_screenshot_node_insert (AsScreenshot *screenshot,
			   GNode *parent,
			   gdouble api_version)
{
	AsImage *image;
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	GNode *n;
	guint i;

	n = as_node_insert (parent, "screenshot", NULL,
			    AS_NODE_INSERT_FLAG_NONE,
			    "type", as_screenshot_kind_to_string (priv->kind),
			    NULL);
	if (api_version >= 0.41) {
		as_node_insert_localized (n,
					  "caption",
					  priv->captions,
					  AS_NODE_INSERT_FLAG_DEDUPE_LANG);
	}
	for (i = 0; i < priv->images->len; i++) {
		image = g_ptr_array_index (priv->images, i);
		as_image_node_insert (image, n, api_version);
	}
	return n;
}

/**
 * as_screenshot_node_parse:
 * @screenshot: a #AsScreenshot instance.
 * @node: a #GNode.
 * @error: A #GError or %NULL.
 *
 * Populates the object from a DOM node.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.1.0
 **/
gboolean
as_screenshot_node_parse (AsScreenshot *screenshot, GNode *node, GError **error)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	GList *l;
	GNode *c;
	const gchar *tmp;
	guint size;
	_cleanup_hashtable_unref_ GHashTable *captions = NULL;

	tmp = as_node_get_attribute (node, "type");
	if (tmp != NULL) {
		as_screenshot_set_kind (screenshot,
					as_screenshot_kind_from_string (tmp));
	}

	/* add captions */
	captions = as_node_get_localized (node, "caption");
	if (captions != NULL) {
		_cleanup_list_free_ GList *keys;
		keys = g_hash_table_get_keys (captions);
		for (l = keys; l != NULL; l = l->next) {
			tmp = l->data;
			as_screenshot_set_caption (screenshot,
						   tmp,
						   g_hash_table_lookup (captions, tmp),
						   -1);
		}
	}

	/* AppData files does not have <image> tags */
	tmp = as_node_get_data (node);
	if (tmp != NULL) {
		AsImage *image;
		image = as_image_new ();
		as_image_set_kind (image, AS_IMAGE_KIND_SOURCE);
		size = as_node_get_attribute_as_int (node, "width");
		if (size != G_MAXUINT)
			as_image_set_width (image, size);
		size = as_node_get_attribute_as_int (node, "height");
		if (size != G_MAXUINT)
			as_image_set_height (image, size);
		as_image_set_url (image, tmp, -1);
		g_ptr_array_add (priv->images, image);
	}

	/* add images */
	for (c = node->children; c != NULL; c = c->next) {
		_cleanup_object_unref_ AsImage *image = NULL;
		if (as_node_get_tag (c) != AS_TAG_IMAGE)
			continue;
		image = as_image_new ();
		if (!as_image_node_parse (image, c, error))
			return FALSE;
		g_ptr_array_add (priv->images, g_object_ref (image));
	}
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
