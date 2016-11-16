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
#include "as-ref-string.h"
#include "as-utils-private.h"
#include "as-yaml.h"

typedef struct
{
	AsImageKind		 kind;
	AsRefString		*locale;
	AsRefString		*url;
	AsRefString		*md5;
	AsRefString		*basename;
	guint			 width;
	guint			 height;
	GdkPixbuf		*pixbuf;
} AsImagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsImage, as_image, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_image_get_instance_private (o))

static void
as_image_finalize (GObject *object)
{
	AsImage *image = AS_IMAGE (object);
	AsImagePrivate *priv = GET_PRIVATE (image);

	if (priv->pixbuf != NULL)
		g_object_unref (priv->pixbuf);
	if (priv->url != NULL)
		as_ref_string_unref (priv->url);
	if (priv->md5 != NULL)
		as_ref_string_unref (priv->md5);
	if (priv->basename != NULL)
		as_ref_string_unref (priv->basename);
	if (priv->locale != NULL)
		as_ref_string_unref (priv->locale);

	G_OBJECT_CLASS (as_image_parent_class)->finalize (object);
}

static void
as_image_init (AsImage *image)
{
}

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
 * as_image_get_basename:
 * @image: a #AsImage instance.
 *
 * Gets the suggested basename the image, including file extension.
 *
 * Returns: filename
 *
 * Since: 0.1.6
 **/
const gchar *
as_image_get_basename (AsImage *image)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	return priv->basename;
}

/**
 * as_image_get_locale:
 * @image: a #AsImage instance.
 *
 * Gets the locale of the image.
 *
 * Returns: locale, or %NULL
 *
 * Since: 0.5.14
 **/
const gchar *
as_image_get_locale (AsImage *image)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	return priv->locale;
}

/**
 * as_image_get_md5:
 * @image: a #AsImage instance.
 *
 * Gets the string representation of the pixbuf hash value.
 *
 * Returns: string representing the MD5 sum, or %NULL if unset
 *
 * Since: 0.1.6
 **/
const gchar *
as_image_get_md5 (AsImage *image)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	return priv->md5;
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
 *
 * Sets the fully-qualified mirror URL to use for the image.
 *
 * Since: 0.1.0
 **/
void
as_image_set_url (AsImage *image, const gchar *url)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	as_ref_string_assign_safe (&priv->url, url);
}

/**
 * as_image_set_basename:
 * @image: a #AsImage instance.
 * @basename: the new filename basename.
 *
 * Sets the image basename filename.
 *
 * Since: 0.1.6
 **/
void
as_image_set_basename (AsImage *image, const gchar *basename)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	as_ref_string_assign_safe (&priv->basename, basename);
}

/**
 * as_image_set_locale:
 * @image: a #AsImage instance.
 * @locale: the new image locale, e.g. "en_GB" or %NULL.
 *
 * Sets the image locale.
 *
 * Since: 0.5.14
 **/
void
as_image_set_locale (AsImage *image, const gchar *locale)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	as_ref_string_assign_safe (&priv->locale, locale);
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
 * @pixbuf: the #GdkPixbuf, or %NULL
 *
 * Sets the image pixbuf.
 *
 * Since: 0.1.6
 **/
void
as_image_set_pixbuf (AsImage *image, GdkPixbuf *pixbuf)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	guchar *data;
	guint len;

	g_set_object (&priv->pixbuf, pixbuf);
	if (pixbuf == NULL)
		return;
	if (priv->md5 == NULL) {
		g_autofree gchar *md5_tmp = NULL;
		data = gdk_pixbuf_get_pixels_with_length (pixbuf, &len);
		md5_tmp = g_compute_checksum_for_data (G_CHECKSUM_MD5,
						       data, len);
		as_ref_string_assign_safe (&priv->md5, md5_tmp);
	}
	priv->width = (guint) gdk_pixbuf_get_width (pixbuf);
	priv->height = (guint) gdk_pixbuf_get_height (pixbuf);
}

/**
 * as_image_node_insert: (skip)
 * @image: a #AsImage instance.
 * @parent: the parent #GNode to use..
 * @ctx: the #AsNodeContext
 *
 * Inserts the image into the DOM tree.
 *
 * Returns: (transfer none): A populated #GNode
 *
 * Since: 0.1.0
 **/
GNode *
as_image_node_insert (AsImage *image, GNode *parent, AsNodeContext *ctx)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	GNode *n;
	n = as_node_insert (parent, "image", priv->url,
			    AS_NODE_INSERT_FLAG_NONE,
			    NULL);
	if (priv->width > 0)
		as_node_add_attribute_as_uint (n, "width", priv->width);
	if (priv->height > 0)
		as_node_add_attribute_as_uint (n, "height", priv->height);
	if (priv->kind > AS_IMAGE_KIND_UNKNOWN)
		as_node_add_attribute (n, "type", as_image_kind_to_string (priv->kind));
	if (priv->locale != NULL)
		as_node_add_attribute (n, "xml:lang", priv->locale);
	return n;
}

/**
 * as_image_node_parse:
 * @image: a #AsImage instance.
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
as_image_node_parse (AsImage *image, GNode *node,
		     AsNodeContext *ctx, GError **error)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	const gchar *tmp;
	guint size;

	size = as_node_get_attribute_as_uint (node, "width");
	if (size != G_MAXUINT)
		as_image_set_width (image, size);
	size = as_node_get_attribute_as_uint (node, "height");
	if (size != G_MAXUINT)
		as_image_set_height (image, size);
	tmp = as_node_get_attribute (node, "type");
	if (tmp == NULL)
		as_image_set_kind (image, AS_IMAGE_KIND_SOURCE);
	else
		as_image_set_kind (image, as_image_kind_from_string (tmp));
	as_ref_string_assign (&priv->url, as_node_get_data (node));
	as_ref_string_assign (&priv->locale, as_node_get_attribute (node, "xml:lang"));
	return TRUE;
}

/**
 * as_image_node_parse_dep11:
 * @image: a #AsImage instance.
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
as_image_node_parse_dep11 (AsImage *im, GNode *node,
			   AsNodeContext *ctx, GError **error)
{
	GNode *n;
	const gchar *tmp;

	for (n = node->children; n != NULL; n = n->next) {
		tmp = as_yaml_node_get_key (n);
		if (g_strcmp0 (tmp, "height") == 0)
			as_image_set_height (im, as_yaml_node_get_value_as_uint (n));
		else if (g_strcmp0 (tmp, "width") == 0)
			as_image_set_width (im, as_yaml_node_get_value_as_uint (n));
		else if (g_strcmp0 (tmp, "url") == 0) {
			const gchar *media_base_url = as_node_context_get_media_base_url (ctx);
			if (media_base_url != NULL) {
				g_autofree gchar *url = NULL;
				url = g_build_path ("/", media_base_url, as_yaml_node_get_value (n), NULL);
				as_image_set_url (im, url);
			} else {
				as_image_set_url (im, as_yaml_node_get_value (n));
			}
		}
	}
	return TRUE;
}

/**
 * as_image_load_filename_full:
 * @image: a #AsImage instance.
 * @filename: filename to read from
 * @dest_size: The size of the constructed pixbuf, or 0 for the native size
 * @src_size_min: The smallest source size allowed, or 0 for none
 * @flags: a #AsImageLoadFlags, e.g. %AS_IMAGE_LOAD_FLAG_NONE
 * @error: A #GError or %NULL.
 *
 * Reads an image from a file.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.5.6
 **/
gboolean
as_image_load_filename_full (AsImage *image,
			     const gchar *filename,
			     guint dest_size,
			     guint src_size_min,
			     AsImageLoadFlags flags,
			     GError **error)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	guint pixbuf_height;
	guint pixbuf_width;
	guint tmp_height;
	guint tmp_width;
	g_autoptr(GdkPixbuf) pixbuf = NULL;
	g_autoptr(GdkPixbuf) pixbuf_src = NULL;
	g_autoptr(GdkPixbuf) pixbuf_tmp = NULL;

	/* only support non-deprecated types */
	if (flags & AS_IMAGE_LOAD_FLAG_ONLY_SUPPORTED) {
		GdkPixbufFormat *fmt;
		fmt = gdk_pixbuf_get_file_info (filename, NULL, NULL);
		if (fmt == NULL) {
			g_set_error_literal (error,
					     AS_UTILS_ERROR,
					     AS_UTILS_ERROR_FAILED,
					     "image format was not recognized");
			return FALSE;
		}
		if (g_strcmp0 (gdk_pixbuf_format_get_name (fmt), "png") != 0 &&
		    g_strcmp0 (gdk_pixbuf_format_get_name (fmt), "jpeg") != 0 &&
		    g_strcmp0 (gdk_pixbuf_format_get_name (fmt), "svg") != 0) {
			g_set_error (error,
				     AS_UTILS_ERROR,
				     AS_UTILS_ERROR_FAILED,
				     "image format %s is not supported",
				     gdk_pixbuf_format_get_name (fmt));
			return FALSE;
		}
	}

	/* update basename */
	if (flags & AS_IMAGE_LOAD_FLAG_SET_BASENAME) {
		g_autofree gchar *basename = NULL;
		basename = g_path_get_basename (filename);
		as_image_set_basename (image, basename);
	}

	/* update checksum */
	if (flags & AS_IMAGE_LOAD_FLAG_SET_CHECKSUM) {
		gsize len;
		g_autofree gchar *data = NULL;
		g_autofree gchar *md5_tmp = NULL;

		/* get the contents so we can hash the predictable file data,
		 * rather than the unpredicatable (for JPEG) pixel data */
		if (!g_file_get_contents (filename, &data, &len, error))
			return FALSE;
		md5_tmp = g_compute_checksum_for_data (G_CHECKSUM_MD5,
						       (guchar * )data, len);
		as_ref_string_assign_safe (&priv->md5, md5_tmp);
	}

	/* load the image of the native size */
	if (dest_size == 0) {
		pixbuf = gdk_pixbuf_new_from_file (filename, error);
		if (pixbuf == NULL)
			return FALSE;
		as_image_set_pixbuf (image, pixbuf);
		return TRUE;
	}

	/* open file in native size */
	if (g_str_has_suffix (filename, ".svg")) {
		pixbuf_src = gdk_pixbuf_new_from_file_at_scale (filename,
								(gint) dest_size,
								(gint) dest_size,
								TRUE, error);
	} else {
		pixbuf_src = gdk_pixbuf_new_from_file (filename, error);
	}
	if (pixbuf_src == NULL)
		return FALSE;

	/* check size */
	if (gdk_pixbuf_get_width (pixbuf_src) < (gint) src_size_min &&
	    gdk_pixbuf_get_height (pixbuf_src) < (gint) src_size_min) {
		g_set_error (error,
			     AS_UTILS_ERROR,
			     AS_UTILS_ERROR_FAILED,
			     "icon was too small %ix%i",
			     gdk_pixbuf_get_width (pixbuf_src),
			     gdk_pixbuf_get_height (pixbuf_src));
		return FALSE;
	}

	/* don't do anything to an icon with the perfect size */
	pixbuf_width = (guint) gdk_pixbuf_get_width (pixbuf_src);
	pixbuf_height = (guint) gdk_pixbuf_get_height (pixbuf_src);
	if (pixbuf_width == dest_size && pixbuf_height == dest_size) {
		as_image_set_pixbuf (image, pixbuf_src);
		return TRUE;
	}

	/* never scale up, just pad */
	if (pixbuf_width < dest_size && pixbuf_height < dest_size) {
		g_debug ("icon padded to %ux%u as size %ux%u",
			 dest_size, dest_size,
			 pixbuf_width, pixbuf_height);
		pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8,
					 (gint) dest_size, (gint) dest_size);
		gdk_pixbuf_fill (pixbuf, 0x00000000);
		gdk_pixbuf_copy_area (pixbuf_src,
				      0, 0, /* of src */
				      (gint) pixbuf_width,
				      (gint) pixbuf_height,
				      pixbuf,
				      (gint) (dest_size - pixbuf_width) / 2,
				      (gint) (dest_size - pixbuf_height) / 2);
		as_image_set_pixbuf (image, pixbuf);
		return TRUE;
	}

	/* is the aspect ratio perfectly square */
	if (pixbuf_width == pixbuf_height) {
		pixbuf = gdk_pixbuf_scale_simple (pixbuf_src,
						  (gint) dest_size,
						  (gint) dest_size,
						  GDK_INTERP_HYPER);
		as_image_set_pixbuf (image, pixbuf);
		return TRUE;
	}

	/* create new square pixbuf with alpha padding */
	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8,
				 (gint) dest_size, (gint) dest_size);
	gdk_pixbuf_fill (pixbuf, 0x00000000);
	if (pixbuf_width > pixbuf_height) {
		tmp_width = dest_size;
		tmp_height = dest_size * pixbuf_height / pixbuf_width;
	} else {
		tmp_width = dest_size * pixbuf_width / pixbuf_height;
		tmp_height = dest_size;
	}
	pixbuf_tmp = gdk_pixbuf_scale_simple (pixbuf_src,
					      (gint) tmp_width,
					      (gint) tmp_height,
					      GDK_INTERP_HYPER);
	if (flags & AS_IMAGE_LOAD_FLAG_SHARPEN)
		as_pixbuf_sharpen (pixbuf_tmp, 1, -0.5);
	gdk_pixbuf_copy_area (pixbuf_tmp,
			      0, 0, /* of src */
			      (gint) tmp_width, (gint) tmp_height,
			      pixbuf,
			      (gint) (dest_size - tmp_width) / 2,
			      (gint) (dest_size - tmp_height) / 2);
	as_image_set_pixbuf (image, pixbuf);
	return TRUE;
}

/**
 * as_image_load_filename:
 * @image: a #AsImage instance.
 * @filename: filename to read from
 * @error: A #GError or %NULL.
 *
 * Reads a pixbuf from a file.
 *
 * NOTE: This function also sets the suggested filename which can be retrieved
 * using as_image_get_basename(). This can be overridden if required.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.1.6
 **/
gboolean
as_image_load_filename (AsImage *image,
			const gchar *filename,
			GError **error)
{
	return as_image_load_filename_full (image,
					    filename,
					    0, 0,
					    AS_IMAGE_LOAD_FLAG_SET_BASENAME |
					    AS_IMAGE_LOAD_FLAG_SET_CHECKSUM,
					    error);
}

/**
 * as_image_save_pixbuf:
 * @image: a #AsImage instance.
 * @width: target width, or 0 for default
 * @height: target height, or 0 for default
 * @flags: some #AsImageSaveFlags values, e.g. %AS_IMAGE_SAVE_FLAG_PAD_16_9
 *
 * Resamples a pixbuf to a specific size.
 *
 * Returns: (transfer full): A #GdkPixbuf of the specified size
 *
 * Since: 0.1.6
 **/
GdkPixbuf *
as_image_save_pixbuf (AsImage *image,
		      guint width,
		      guint height,
		      AsImageSaveFlags flags)
{
	AsImagePrivate *priv = GET_PRIVATE (image);
	GdkPixbuf *pixbuf = NULL;
	guint tmp_height;
	guint tmp_width;
	guint pixbuf_height;
	guint pixbuf_width;
	g_autoptr(GdkPixbuf) pixbuf_tmp = NULL;

	/* never set */
	if (priv->pixbuf == NULL)
		return NULL;

	/* 0 means 'default' */
	if (width == 0)
		width = (guint) gdk_pixbuf_get_width (priv->pixbuf);
	if (height == 0)
		height = (guint) gdk_pixbuf_get_height (priv->pixbuf);

	/* don't do anything to an image with the correct size */
	pixbuf_width = (guint) gdk_pixbuf_get_width (priv->pixbuf);
	pixbuf_height = (guint) gdk_pixbuf_get_height (priv->pixbuf);
	if (width == pixbuf_width && height == pixbuf_height)
		return g_object_ref (priv->pixbuf);

	/* is the aspect ratio of the source perfectly 16:9 */
	if (flags == AS_IMAGE_SAVE_FLAG_NONE ||
	    (pixbuf_width / 16) * 9 == pixbuf_height) {
		pixbuf = gdk_pixbuf_scale_simple (priv->pixbuf,
						  (gint) width, (gint) height,
						  GDK_INTERP_HYPER);
		if ((flags & AS_IMAGE_SAVE_FLAG_SHARPEN) > 0)
			as_pixbuf_sharpen (pixbuf, 1, -0.5);
		if ((flags & AS_IMAGE_SAVE_FLAG_BLUR) > 0)
			as_pixbuf_blur (pixbuf, 5, 3);
		return pixbuf;
	}

	/* create new 16:9 pixbuf with alpha padding */
	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
				 TRUE, 8,
				 (gint) width,
				 (gint) height);
	gdk_pixbuf_fill (pixbuf, 0x00000000);
	if ((pixbuf_width / 16) * 9 > pixbuf_height) {
		tmp_width = width;
		tmp_height = width * pixbuf_height / pixbuf_width;
	} else {
		tmp_width = height * pixbuf_width / pixbuf_height;
		tmp_height = height;
	}
	pixbuf_tmp = gdk_pixbuf_scale_simple (priv->pixbuf,
					      (gint) tmp_width,
					      (gint) tmp_height,
					      GDK_INTERP_HYPER);
	if ((flags & AS_IMAGE_SAVE_FLAG_SHARPEN) > 0)
		as_pixbuf_sharpen (pixbuf_tmp, 1, -0.5);
	if ((flags & AS_IMAGE_SAVE_FLAG_BLUR) > 0)
		as_pixbuf_blur (pixbuf_tmp, 5, 3);
	gdk_pixbuf_copy_area (pixbuf_tmp,
			      0, 0, /* of src */
			      (gint) tmp_width,
			      (gint) tmp_height,
			      pixbuf,
			      (gint) (width - tmp_width) / 2,
			      (gint) (height - tmp_height) / 2);
	return pixbuf;
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
	g_autoptr(GdkPixbuf) pixbuf = NULL;

	/* save source file */
	pixbuf = as_image_save_pixbuf (image, width, height, flags);
	return gdk_pixbuf_save (pixbuf,
				filename,
				"png",
				error,
				NULL);
}

static gboolean
is_pixel_alpha (GdkPixbuf *pixbuf, guint x, guint y)
{
	guint rowstride, n_channels;
	guchar *pixels, *p;

	n_channels = (guint) gdk_pixbuf_get_n_channels (pixbuf);
	rowstride = (guint) gdk_pixbuf_get_rowstride (pixbuf);
	pixels = gdk_pixbuf_get_pixels (pixbuf);

	p = pixels + y * rowstride + x * n_channels;
	return p[3] == 0;
}

typedef enum {
	AS_IMAGE_ALPHA_MODE_START,
	AS_IMAGE_ALPHA_MODE_PADDING,
	AS_IMAGE_ALPHA_MODE_CONTENT,
} AsImageAlphaMode;

/**
 * as_image_get_alpha_flags: (skip)
 * @image: a #AsImage instance.
 *
 * Gets the alpha flags for the image. The following image would have all flags
 * set, where 'x' is alpha and '@' is non-alpha.
 *
 * xxxxxxxxxxxxxxxxxxxxxxxxxxxx
 * xx@@@@@@@@@@@@@@@@@@@@@@@@xx
 * xx@@@@@@@xxxxxx@@@@@@@@@@@xx
 * xx@@@@@@@xxxxxx@@@@@@@@@@@xx
 * xx@@@@@@@@@@@@@@@@@@@@@@@@xx
 * xxxxxxxxxxxxxxxxxxxxxxxxxxxx
 *
 * Returns: #AsImageAlphaFlags, e.g. %AS_IMAGE_ALPHA_FLAG_LEFT
 *
 * Since: 0.2.2
 **/
AsImageAlphaFlags
as_image_get_alpha_flags (AsImage *image)
{
	AsImageAlphaFlags flags = AS_IMAGE_ALPHA_FLAG_TOP |
				  AS_IMAGE_ALPHA_FLAG_BOTTOM |
				  AS_IMAGE_ALPHA_FLAG_LEFT |
				  AS_IMAGE_ALPHA_FLAG_RIGHT;
	AsImageAlphaMode mode_h;
	AsImageAlphaMode mode_v = AS_IMAGE_ALPHA_MODE_START;
	AsImagePrivate *priv = GET_PRIVATE (image);
	gboolean complete_line_of_alpha;
	gboolean is_alpha;
	guint width, height;
	guint x, y;
	guint cnt_content_to_alpha_h;
	guint cnt_content_to_alpha_v = 0;

	if (!gdk_pixbuf_get_has_alpha (priv->pixbuf))
		return AS_IMAGE_ALPHA_FLAG_NONE;

	width = (guint) gdk_pixbuf_get_width (priv->pixbuf);
	height = (guint) gdk_pixbuf_get_height (priv->pixbuf);
	for (y = 0; y < height; y++) {
		mode_h = AS_IMAGE_ALPHA_MODE_START;
		complete_line_of_alpha = TRUE;
		cnt_content_to_alpha_h = 0;
		for (x = 0; x < width; x++) {
			is_alpha = is_pixel_alpha (priv->pixbuf, x, y);
			/* use the frame */
			if (!is_alpha) {
				if (x == 0)
					flags &= ~AS_IMAGE_ALPHA_FLAG_LEFT;
				if (x == width - 1)
					flags &= ~AS_IMAGE_ALPHA_FLAG_RIGHT;
				if (y == 0)
					flags &= ~AS_IMAGE_ALPHA_FLAG_TOP;
				if (y == height - 1)
					flags &= ~AS_IMAGE_ALPHA_FLAG_BOTTOM;
				complete_line_of_alpha = FALSE;
			}

			/* use line state machine */
			switch (mode_h) {
			case AS_IMAGE_ALPHA_MODE_START:
				mode_h = is_alpha ? AS_IMAGE_ALPHA_MODE_PADDING :
						    AS_IMAGE_ALPHA_MODE_CONTENT;
				break;
			case AS_IMAGE_ALPHA_MODE_PADDING:
				if (!is_alpha)
					mode_h = AS_IMAGE_ALPHA_MODE_CONTENT;
				break;
			case AS_IMAGE_ALPHA_MODE_CONTENT:
				if (is_alpha) {
					mode_h = AS_IMAGE_ALPHA_MODE_PADDING;
					cnt_content_to_alpha_h++;
				}
				break;
			default:
				g_assert_not_reached ();
			}
		}

		/* use column state machine */
		switch (mode_v) {
		case AS_IMAGE_ALPHA_MODE_START:
			if (complete_line_of_alpha) {
				mode_v = AS_IMAGE_ALPHA_MODE_PADDING;
			} else {
				mode_v = AS_IMAGE_ALPHA_MODE_CONTENT;
			}
			break;
		case AS_IMAGE_ALPHA_MODE_PADDING:
			if (!complete_line_of_alpha)
				mode_v = AS_IMAGE_ALPHA_MODE_CONTENT;
			break;
		case AS_IMAGE_ALPHA_MODE_CONTENT:
			if (complete_line_of_alpha) {
				mode_v = AS_IMAGE_ALPHA_MODE_PADDING;
				cnt_content_to_alpha_v++;
			}
			break;
		default:
			g_assert_not_reached ();
		}

		/* detect internal alpha */
		if (mode_h == AS_IMAGE_ALPHA_MODE_PADDING) {
			if (cnt_content_to_alpha_h >= 2)
				flags |= AS_IMAGE_ALPHA_FLAG_INTERNAL;
		} else if (mode_h == AS_IMAGE_ALPHA_MODE_CONTENT) {
			if (cnt_content_to_alpha_h >= 1)
				flags |= AS_IMAGE_ALPHA_FLAG_INTERNAL;
		}
	}

	/* detect internal alpha */
	if (mode_v == AS_IMAGE_ALPHA_MODE_PADDING) {
		if (cnt_content_to_alpha_v >= 2)
			flags |= AS_IMAGE_ALPHA_FLAG_INTERNAL;
	} else if (mode_v == AS_IMAGE_ALPHA_MODE_CONTENT) {
		if (cnt_content_to_alpha_v >= 1)
			flags |= AS_IMAGE_ALPHA_FLAG_INTERNAL;
	}
	return flags;
}

/**
 * as_image_equal:
 * @image1: a #AsImage instance.
 * @image2: a #AsImage instance.
 *
 * Checks if two images are the same.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.5.7
 **/
gboolean
as_image_equal (AsImage *image1, AsImage *image2)
{
	AsImagePrivate *priv1 = GET_PRIVATE (image1);
	AsImagePrivate *priv2 = GET_PRIVATE (image2);

	/* trivial */
	if (image1 == image2)
		return TRUE;

	/* check for equality */
	if (priv1->kind != priv2->kind)
		return FALSE;
	if (priv1->width != priv2->width)
		return FALSE;
	if (priv1->height != priv2->height)
		return FALSE;
	if (g_strcmp0 (priv1->url, priv2->url) != 0)
		return FALSE;
	if (g_strcmp0 (priv1->md5, priv2->md5) != 0)
		return FALSE;

	/* success */
	return TRUE;
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
 
