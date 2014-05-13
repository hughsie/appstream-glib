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
 * SECTION:as-image
 * @short_description: Object representing a single image used in a screenshot.
 * @include: appstream-glib.h
 * @stability: Stable
 *
 * Screenshot may have multiple versions of an image in different resolutions
 * or aspect ratios. This object allows access to the location and size of a
 * single image.
 *
 * See also: #AsScreenshot
 */

#include "config.h"

#include "as-image-private.h"
#include "as-node-private.h"
#include "as-utils-private.h"

typedef struct _AsImagePrivate	AsImagePrivate;
struct _AsImagePrivate
{
	AsImageKind		 kind;
	gchar			*url;
	guint			 width;
	guint			 height;
	GdkPixbuf		*pixbuf;
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

	if (priv->pixbuf != NULL)
		g_object_unref (priv->pixbuf);
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
 * @kind: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: (transfer full): a #AsImageKind, or %AS_IMAGE_KIND_UNKNOWN for unknown.
 *
 * Since: 0.1.0
 **/
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
 * @kind: the #AsImageKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @kind
 *
 * Since: 0.1.0
 **/
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
 * @image: a #AsImage instance.
 *
 * Gets the full qualified URL for the image, usually pointing at some mirror.
 *
 * Returns: URL
 *
 * Since: 0.1.0
 **/
const gchar *
as_image_get_url (AsImage *image)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	return priv->url;
}

/**
 * as_image_get_width:
 * @image: a #AsImage instance.
 *
 * Gets the image width.
 *
 * Returns: width in pixels
 *
 * Since: 0.1.0
 **/
guint
as_image_get_width (AsImage *image)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	return priv->width;
}

/**
 * as_image_get_height:
 * @image: a #AsImage instance.
 *
 * Gets the image height.
 *
 * Returns: height in pixels
 *
 * Since: 0.1.0
 **/
guint
as_image_get_height (AsImage *image)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	return priv->height;
}

/**
 * as_image_get_kind:
 * @image: a #AsImage instance.
 *
 * Gets the image kind.
 *
 * Returns: the #AsImageKind
 *
 * Since: 0.1.0
 **/
AsImageKind
as_image_get_kind (AsImage *image)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	return priv->kind;
}

/**
 * as_image_get_pixbuf:
 * @image: a #AsImage instance.
 *
 * Gets the image pixbuf if set.
 *
 * Returns: (transfer none): the #GdkPixbuf, or %NULL
 *
 * Since: 0.1.6
 **/
GdkPixbuf *
as_image_get_pixbuf (AsImage *image)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	return priv->pixbuf;
}

/**
 * as_image_set_url:
 * @image: a #AsImage instance.
 * @url: the URL.
 * @url_len: the size of @url, or -1 if %NULL-terminated.
 *
 * Sets the fully-qualified mirror URL to use for the image.
 *
 * Since: 0.1.0
 **/
void
as_image_set_url (AsImage *image, const gchar *url, gssize url_len)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	g_free (priv->url);
	priv->url = as_strndup (url, url_len);
}

/**
 * as_image_set_width:
 * @image: a #AsImage instance.
 * @width: the width in pixels.
 *
 * Sets the image width.
 *
 * Since: 0.1.0
 **/
void
as_image_set_width (AsImage *image, guint width)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	priv->width = width;
}

/**
 * as_image_set_height:
 * @image: a #AsImage instance.
 * @height: the height in pixels.
 *
 * Sets the image height.
 *
 * Since: 0.1.0
 **/
void
as_image_set_height (AsImage *image, guint height)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	priv->height = height;
}

/**
 * as_image_set_kind:
 * @image: a #AsImage instance.
 * @kind: the #AsImageKind, e.g. %AS_IMAGE_KIND_THUMBNAIL.
 *
 * Sets the image kind.
 *
 * Since: 0.1.0
 **/
void
as_image_set_kind (AsImage *image, AsImageKind kind)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	priv->kind = kind;
}

/**
 * as_image_set_pixbuf:
 * @image: a #AsImage instance.
 * @pixbuf: the #GdkPixbuf
 *
 * Sets the image pixbuf.
 *
 * Since: 0.1.6
 **/
void
as_image_set_pixbuf (AsImage *image, GdkPixbuf *pixbuf)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	if (priv->pixbuf != NULL)
		g_object_unref (priv->pixbuf);
	priv->pixbuf = g_object_ref (pixbuf);
}

/**
 * as_image_node_insert: (skip)
 * @image: a #AsImage instance.
 * @parent: the parent #GNode to use..
 * @api_version: the AppStream API version
 *
 * Inserts the image into the DOM tree.
 *
 * Returns: (transfer full): A populated #GNode
 *
 * Since: 0.1.0
 **/
GNode *
as_image_node_insert (AsImage *image, GNode *parent, gdouble api_version)
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
 * @image: a #AsImage instance.
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
as_image_node_parse (AsImage *image, GNode *node, GError **error)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	const gchar *tmp;
	gchar *taken;
	guint size;

	size = as_node_get_attribute_as_int (node, "width");
	if (size != G_MAXUINT)
		as_image_set_width (image, size);
	size = as_node_get_attribute_as_int (node, "height");
	if (size != G_MAXUINT)
		as_image_set_height (image, size);
	tmp = as_node_get_attribute (node, "type");
	if (tmp != NULL)
		as_image_set_kind (image, as_image_kind_from_string (tmp));
	taken = as_node_take_data (node);
	if (taken != NULL) {
		g_free (priv->url);
		priv->url = taken;
	}
	return TRUE;
}

/**
 * as_image_save_filename:
 * @image: a #AsImage instance.
 * @filename: filename to write to
 * @width: target width, or 0 for default
 * @height: target height, or 0 for default
 * @flags: some #AsImageSaveFlags values, e.g. %AS_IMAGE_SAVE_FLAG_PAD_16_9
 * @error: A #GError or %NULL.
 *
 * Saves a pixbuf to a file.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.1.6
 **/
gboolean
as_image_save_filename (AsImage *image,
		        const gchar *filename,
		        guint width,
		        guint height,
		        AsImageSaveFlags flags,
		        GError **error)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	GdkPixbuf *pixbuf = NULL;
	GdkPixbuf *pixbuf_tmp = NULL;
	gboolean ret;
	guint tmp_height;
	guint tmp_width;
	guint pixbuf_height;
	guint pixbuf_width;

	/* 0 means 'default' */
	if (width == 0)
		width = gdk_pixbuf_get_width (priv->pixbuf);
	if (height == 0)
		height = gdk_pixbuf_get_height (priv->pixbuf);

	/* is the aspect ratio of the source perfectly 16:9 */
	pixbuf_width = gdk_pixbuf_get_width (priv->pixbuf);
	pixbuf_height = gdk_pixbuf_get_height (priv->pixbuf);
	if (flags == AS_IMAGE_SAVE_FLAG_NONE ||
	    (pixbuf_width / 16) * 9 == pixbuf_height) {
		pixbuf = gdk_pixbuf_scale_simple (priv->pixbuf,
						  width, height,
						  GDK_INTERP_BILINEAR);
	} else {
		/* create new 16:9 pixbuf with alpha padding */
		pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
					 TRUE, 8,
					 width,
					 height);
		gdk_pixbuf_fill (pixbuf, 0x00000000);
		if ((pixbuf_width / 16) * 9 > pixbuf_height) {
			tmp_width = width;
			tmp_height = width * pixbuf_height / pixbuf_width;
		} else {
			tmp_width = height * pixbuf_width / pixbuf_height;
			tmp_height = height;
		}
		pixbuf_tmp = gdk_pixbuf_scale_simple (priv->pixbuf,
						      tmp_width, tmp_height,
						      GDK_INTERP_BILINEAR);
		gdk_pixbuf_copy_area (pixbuf_tmp,
				      0, 0, /* of src */
				      tmp_width, tmp_height,
				      pixbuf,
				      (width - tmp_width) / 2,
				      (height - tmp_height) / 2);
	}

	/* save source file */
	ret = gdk_pixbuf_save (pixbuf,
			       filename,
			       "png",
			       error,
			       NULL);
	if (!ret)
		goto out;
out:
	if (pixbuf != NULL)
		g_object_unref (pixbuf);
	if (pixbuf_tmp != NULL)
		g_object_unref (pixbuf_tmp);
	return ret;
}

/**
 * as_image_new:
 *
 * Creates a new #AsImage.
 *
 * Returns: (transfer full): a #AsImage
 *
 * Since: 0.1.0
 **/
AsImage *
as_image_new (void)
{
	AsImage *image;
	image = g_object_new (AS_TYPE_IMAGE, NULL);
	return AS_IMAGE (image);
}
