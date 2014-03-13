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

#include "config.h"

#include <string.h>

#include "as-node.h"
#include "as-screenshot.h"
#include "as-tag.h"

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
 */
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
 */
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
 */
AsScreenshotKind
as_screenshot_get_kind (AsScreenshot *screenshot)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	return priv->kind;
}

/**
 * as_screenshot_get_images:
 */
GPtrArray *
as_screenshot_get_images (AsScreenshot *screenshot)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	return priv->images;
}

/**
 * as_screenshot_get_caption:
 */
const gchar *
as_screenshot_get_caption (AsScreenshot *screenshot, const gchar *locale)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	if (locale == NULL)
		locale = "C";
	return g_hash_table_lookup (priv->captions, locale);
}

/**
 * as_screenshot_set_kind:
 */
void
as_screenshot_set_kind (AsScreenshot *screenshot, AsScreenshotKind kind)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	priv->kind = kind;
}

/**
 * as_screenshot_add_image:
 */
void
as_screenshot_add_image (AsScreenshot *screenshot, AsImage *image)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	g_ptr_array_add (priv->images, g_object_ref (image));
}

/**
 * as_screenshot_set_caption:
 */
void
as_screenshot_set_caption (AsScreenshot *screenshot,
			   const gchar *locale,
			   const gchar *caption,
			   gsize caption_length)
{
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	if (locale == NULL)
		locale = "C";
	if (caption_length == (gsize) -1)
		caption_length = strlen (caption);
	g_hash_table_insert (priv->captions,
			     g_strdup (locale),
			     g_strndup (caption, caption_length));
}

/**
 * as_screenshot_node_insert:
 **/
GNode *
as_screenshot_node_insert (AsScreenshot *screenshot, GNode *parent)
{
	AsImage *image;
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	GNode *n;
	guint i;

	n = as_node_insert (parent, "screenshot", NULL,
			    AS_NODE_INSERT_FLAG_NONE,
			    "type", as_screenshot_kind_to_string (priv->kind),
			    NULL);
	as_node_insert_localized (n, "caption", priv->captions, 0);
	for (i = 0; i < priv->images->len; i++) {
		image = g_ptr_array_index (priv->images, i);
		as_image_node_insert (image, n);
	}
	return n;
}

/**
 * as_screenshot_node_parse:
 **/
gboolean
as_screenshot_node_parse (AsScreenshot *screenshot, GNode *node, GError **error)
{
	AsImage *image;
	AsScreenshotPrivate *priv = GET_PRIVATE (screenshot);
	GHashTable *captions = NULL;
	GList *keys;
	GList *l;
	GNode *c;
	const gchar *tmp;
	gboolean ret = TRUE;

	tmp = as_node_get_attribute (node, "type");
	if (tmp != NULL) {
		as_screenshot_set_kind (screenshot,
					as_screenshot_kind_from_string (tmp));
	}

	/* add captions */
	captions = as_node_get_localized (node, "caption");
	if (captions != NULL) {
		keys = g_hash_table_get_keys (captions);
		for (l = keys; l != NULL; l = l->next) {
			tmp = l->data;
			as_screenshot_set_caption (screenshot,
						   tmp,
						   g_hash_table_lookup (captions, tmp),
						   -1);
		}
		g_list_free (keys);
	}

	/* add images */
	for (c = node->children; c != NULL; c = c->next) {
		if (as_tag_from_string (as_node_get_name (c)) != AS_TAG_IMAGE)
			continue;
		image = as_image_new ();
		ret = as_image_node_parse (image, c, error);
		if (!ret) {
			g_object_unref (image);
			goto out;
		}
		g_ptr_array_add (priv->images, image);
	}
out:
	if (captions != NULL)
		g_hash_table_unref (captions);
	return ret;
}

/**
 * as_screenshot_new:
 **/
AsScreenshot *
as_screenshot_new (void)
{
	AsScreenshot *screenshot;
	screenshot = g_object_new (AS_TYPE_SCREENSHOT, NULL);
	return AS_SCREENSHOT (screenshot);
}
