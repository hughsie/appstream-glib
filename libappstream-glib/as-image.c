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

#include <stdlib.h>

#include "as-image.h"
#include "as-node.h"
#include "as-utils.h"

typedef struct _AsImagePrivate	AsImagePrivate;
struct _AsImagePrivate
{
	AsImageKind		 kind;
	gchar			*url;
	guint			 width;
	guint			 height;
};

G_DEFINE_TYPE_WITH_PRIVATE (AsImage, as_image, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_image_get_instance_private (o))

/**
 * as_image_finalize:
 **/
static void
as_image_finalize (GObject *object)
{
	AsImage *image = AS_IMAGE (object);
	AsImagePrivate *priv = GET_PRIVATE (image);

	g_free (priv->url);

	G_OBJECT_CLASS (as_image_parent_class)->finalize (object);
}

/**
 * as_image_init:
 **/
static void
as_image_init (AsImage *image)
{
}

/**
 * as_image_class_init:
 **/
static void
as_image_class_init (AsImageClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_image_finalize;
}


/**
 * as_image_kind_from_string:
 */
AsImageKind
as_image_kind_from_string (const gchar *kind)
{
	if (g_strcmp0 (kind, "source") == 0)
		return AS_IMAGE_KIND_SOURCE;
	if (g_strcmp0 (kind, "thumbnail") == 0)
		return AS_IMAGE_KIND_THUMBNAIL;
	return AS_IMAGE_KIND_UNKNOWN;
}

/**
 * as_image_kind_to_string:
 */
const gchar *
as_image_kind_to_string (AsImageKind kind)
{
	if (kind == AS_IMAGE_KIND_SOURCE)
		return "source";
	if (kind == AS_IMAGE_KIND_THUMBNAIL)
		return "thumbnail";
	return NULL;
}

/**
 * as_image_get_url:
 */
const gchar *
as_image_get_url (AsImage *image)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	return priv->url;
}

/**
 * as_image_get_width:
 */
guint
as_image_get_width (AsImage *image)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	return priv->width;
}

/**
 * as_image_get_height:
 */
guint
as_image_get_height (AsImage *image)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	return priv->height;
}

/**
 * as_image_get_kind:
 */
AsImageKind
as_image_get_kind (AsImage *image)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	return priv->kind;
}

/**
 * as_image_set_url:
 */
void
as_image_set_url (AsImage *image, const gchar *url, gssize url_len)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	g_free (priv->url);
	priv->url = as_strndup (url, url_len);
}

/**
 * as_image_set_width:
 */
void
as_image_set_width (AsImage *image, guint width)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	priv->width = width;
}

/**
 * as_image_set_height:
 */
void
as_image_set_height (AsImage *image, guint height)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	priv->height = height;
}

/**
 * as_image_set_kind:
 */
void
as_image_set_kind (AsImage *image, AsImageKind kind)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	priv->kind = kind;
}

/**
 * as_image_node_insert:
 **/
GNode *
as_image_node_insert (AsImage *image, GNode *parent)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	GNode *n;
	gchar tmp_height[6];
	gchar tmp_width[6];

	if (priv->width > 0 && priv->height > 0) {
		g_snprintf (tmp_width, sizeof (tmp_width), "%u", priv->width);
		g_snprintf (tmp_height, sizeof (tmp_height), "%u", priv->height);
		n = as_node_insert (parent, "image", priv->url,
				    AS_NODE_INSERT_FLAG_NONE,
				    "width", tmp_width,
				    "height", tmp_height,
				    "type", as_image_kind_to_string (priv->kind),
				    NULL);
	} else {
		n = as_node_insert (parent, "image", priv->url,
				    AS_NODE_INSERT_FLAG_NONE,
				    "type", as_image_kind_to_string (priv->kind),
				    NULL);
	}
	return n;
}

/**
 * as_image_node_parse:
 **/
gboolean
as_image_node_parse (AsImage *image, GNode *node, GError **error)
{
	const gchar *tmp;
	tmp = as_node_get_attribute (node, "width");
	if (tmp != NULL)
		as_image_set_width (image, atoi (tmp));
	tmp = as_node_get_attribute (node, "height");
	if (tmp != NULL)
		as_image_set_height (image, atoi (tmp));
	tmp = as_node_get_attribute (node, "type");
	if (tmp != NULL)
		as_image_set_kind (image, as_image_kind_from_string (tmp));
	tmp = as_node_get_data (node);
	if (tmp != NULL)
		as_image_set_url (image, tmp, -1);
	return TRUE;
}

/**
 * as_image_new:
 **/
AsImage *
as_image_new (void)
{
	AsImage *image;
	image = g_object_new (AS_TYPE_IMAGE, NULL);
	return AS_IMAGE (image);
}
