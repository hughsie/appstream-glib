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
 * SECTION:as-icon
 * @short_description: Object representing a single icon used in a screenshot.
 * @include: appstream-glib.h
 * @stability: Stable
 *
 * Screenshot may have multiple versions of an icon in different resolutions
 * or aspect ratios. This object allows access to the location and size of a
 * single icon.
 *
 * See also: #AsScreenshot
 */

#include "config.h"

#include "as-cleanup.h"
#include "as-icon-private.h"
#include "as-node-private.h"
#include "as-utils-private.h"
#include "as-yaml.h"

typedef struct _AsIconPrivate	AsIconPrivate;
struct _AsIconPrivate
{
	AsIconKind		 kind;
	gchar			*name;
	gchar			*prefix;
	gchar			*prefix_private;
	guint			 width;
	guint			 height;
	GdkPixbuf		*pixbuf;
	GBytes			*data;
};

G_DEFINE_TYPE_WITH_PRIVATE (AsIcon, as_icon, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_icon_get_instance_private (o))

/**
 * as_icon_error_quark:
 *
 * Return value: An error quark.
 *
 * Since: 0.3.1
 **/
GQuark
as_icon_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("AsIconError");
	return quark;
}

/**
 * as_icon_finalize:
 **/
static void
as_icon_finalize (GObject *object)
{
	AsIcon *icon = AS_ICON (object);
	AsIconPrivate *priv = GET_PRIVATE (icon);

	if (priv->pixbuf != NULL)
		g_object_unref (priv->pixbuf);
	if (priv->data != NULL)
		g_bytes_unref (priv->data);
	g_free (priv->name);
	g_free (priv->prefix);
	g_free (priv->prefix_private);

	G_OBJECT_CLASS (as_icon_parent_class)->finalize (object);
}

/**
 * as_icon_init:
 **/
static void
as_icon_init (AsIcon *icon)
{
}

/**
 * as_icon_class_init:
 **/
static void
as_icon_class_init (AsIconClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_icon_finalize;
}


/**
 * as_icon_kind_to_string:
 * @icon_kind: the @AsIconKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @icon_kind
 *
 * Since: 0.1.0
 **/
const gchar *
as_icon_kind_to_string (AsIconKind icon_kind)
{
	if (icon_kind == AS_ICON_KIND_CACHED)
		return "cached";
	if (icon_kind == AS_ICON_KIND_STOCK)
		return "stock";
	if (icon_kind == AS_ICON_KIND_REMOTE)
		return "remote";
	if (icon_kind == AS_ICON_KIND_EMBEDDED)
		return "embedded";
	if (icon_kind == AS_ICON_KIND_LOCAL)
		return "local";
	return "unknown";
}

/**
 * as_icon_kind_from_string:
 * @icon_kind: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AsIconKind or %AS_ICON_KIND_UNKNOWN for unknown
 *
 * Since: 0.1.0
 **/
AsIconKind
as_icon_kind_from_string (const gchar *icon_kind)
{
	if (g_strcmp0 (icon_kind, "cached") == 0)
		return AS_ICON_KIND_CACHED;
	if (g_strcmp0 (icon_kind, "stock") == 0)
		return AS_ICON_KIND_STOCK;
	if (g_strcmp0 (icon_kind, "remote") == 0)
		return AS_ICON_KIND_REMOTE;
	if (g_strcmp0 (icon_kind, "embedded") == 0)
		return AS_ICON_KIND_EMBEDDED;
	if (g_strcmp0 (icon_kind, "local") == 0)
		return AS_ICON_KIND_LOCAL;
	return AS_ICON_KIND_UNKNOWN;
}

/**
 * as_icon_get_name:
 * @icon: a #AsIcon instance.
 *
 * Gets the full qualified URL for the icon, usually pointing at some mirror.
 *
 * Returns: URL
 *
 * Since: 0.3.1
 **/
const gchar *
as_icon_get_name (AsIcon *icon)
{
	AsIconPrivate *priv = GET_PRIVATE (icon);
	return priv->name;
}

/**
 * as_icon_get_prefix:
 * @icon: a #AsIcon instance.
 *
 * Gets the suggested prefix of the icon.
 *
 * Returns: filename
 *
 * Since: 0.1.6
 **/
const gchar *
as_icon_get_prefix (AsIcon *icon)
{
	AsIconPrivate *priv = GET_PRIVATE (icon);
	if (priv->prefix_private != NULL)
		return priv->prefix_private;
	return priv->prefix;
}

/**
 * as_icon_get_width:
 * @icon: a #AsIcon instance.
 *
 * Gets the icon width.
 *
 * Returns: width in pixels
 *
 * Since: 0.3.1
 **/
guint
as_icon_get_width (AsIcon *icon)
{
	AsIconPrivate *priv = GET_PRIVATE (icon);
	return priv->width;
}

/**
 * as_icon_get_height:
 * @icon: a #AsIcon instance.
 *
 * Gets the icon height.
 *
 * Returns: height in pixels
 *
 * Since: 0.3.1
 **/
guint
as_icon_get_height (AsIcon *icon)
{
	AsIconPrivate *priv = GET_PRIVATE (icon);
	return priv->height;
}

/**
 * as_icon_get_kind:
 * @icon: a #AsIcon instance.
 *
 * Gets the icon kind.
 *
 * Returns: the #AsIconKind
 *
 * Since: 0.3.1
 **/
AsIconKind
as_icon_get_kind (AsIcon *icon)
{
	AsIconPrivate *priv = GET_PRIVATE (icon);
	return priv->kind;
}

/**
 * as_icon_get_pixbuf:
 * @icon: a #AsIcon instance.
 *
 * Gets the icon pixbuf if set.
 *
 * Returns: (transfer none): the #GdkPixbuf, or %NULL
 *
 * Since: 0.3.1
 **/
GdkPixbuf *
as_icon_get_pixbuf (AsIcon *icon)
{
	AsIconPrivate *priv = GET_PRIVATE (icon);
	return priv->pixbuf;
}

/**
 * as_icon_get_data:
 * @icon: a #AsIcon instance.
 *
 * Gets the icon data if set.
 *
 * Returns: (transfer none): the #GBytes, or %NULL
 *
 * Since: 0.3.1
 **/
GBytes *
as_icon_get_data (AsIcon *icon)
{
	AsIconPrivate *priv = GET_PRIVATE (icon);
	return priv->data;
}

/**
 * as_icon_set_name:
 * @icon: a #AsIcon instance.
 * @name: the URL.
 * @name_len: the size of @name, or -1 if %NULL-terminated.
 *
 * Sets the fully-qualified mirror URL to use for the icon.
 *
 * Since: 0.3.1
 **/
void
as_icon_set_name (AsIcon *icon, const gchar *name, gssize name_len)
{
	AsIconPrivate *priv = GET_PRIVATE (icon);
	g_free (priv->name);
	priv->name = as_strndup (name, name_len);
}

/**
 * as_icon_set_prefix:
 * @icon: a #AsIcon instance.
 * @prefix: the new filename prefix.
 *
 * Sets the icon prefix filename.
 *
 * Since: 0.1.6
 **/
void
as_icon_set_prefix (AsIcon *icon, const gchar *prefix)
{
	AsIconPrivate *priv = GET_PRIVATE (icon);
	g_free (priv->prefix);
	priv->prefix = g_strdup (prefix);
}

/**
 * as_icon_set_width:
 * @icon: a #AsIcon instance.
 * @width: the width in pixels.
 *
 * Sets the icon width.
 *
 * Since: 0.3.1
 **/
void
as_icon_set_width (AsIcon *icon, guint width)
{
	AsIconPrivate *priv = GET_PRIVATE (icon);
	priv->width = width;
}

/**
 * as_icon_set_height:
 * @icon: a #AsIcon instance.
 * @height: the height in pixels.
 *
 * Sets the icon height.
 *
 * Since: 0.3.1
 **/
void
as_icon_set_height (AsIcon *icon, guint height)
{
	AsIconPrivate *priv = GET_PRIVATE (icon);
	priv->height = height;
}

/**
 * as_icon_set_kind:
 * @icon: a #AsIcon instance.
 * @kind: the #AsIconKind, e.g. %AS_ICON_KIND_STOCK.
 *
 * Sets the icon kind.
 *
 * Since: 0.3.1
 **/
void
as_icon_set_kind (AsIcon *icon, AsIconKind kind)
{
	AsIconPrivate *priv = GET_PRIVATE (icon);
	priv->kind = kind;
}

/**
 * as_icon_set_pixbuf:
 * @icon: a #AsIcon instance.
 * @pixbuf: the #GdkPixbuf, or %NULL
 *
 * Sets the icon pixbuf.
 *
 * Since: 0.3.1
 **/
void
as_icon_set_pixbuf (AsIcon *icon, GdkPixbuf *pixbuf)
{
	AsIconPrivate *priv = GET_PRIVATE (icon);

	if (priv->pixbuf != NULL)
		g_object_unref (priv->pixbuf);
	if (pixbuf == NULL) {
		priv->pixbuf = NULL;
		return;
	}
	priv->pixbuf = g_object_ref (pixbuf);
	priv->width = gdk_pixbuf_get_width (pixbuf);
	priv->height = gdk_pixbuf_get_height (pixbuf);
}

/**
 * as_icon_set_data:
 * @icon: a #AsIcon instance.
 * @data: the #GBytes, or %NULL
 *
 * Sets the icon data.
 *
 * Since: 0.3.1
 **/
void
as_icon_set_data (AsIcon *icon, GBytes *data)
{
	AsIconPrivate *priv = GET_PRIVATE (icon);

	if (priv->data != NULL)
		g_bytes_unref (priv->data);
	if (data == NULL) {
		priv->data = NULL;
		return;
	}
	priv->data = g_bytes_ref (data);
}

/**
 * as_icon_node_insert: (skip)
 * @icon: a #AsIcon instance.
 * @parent: the parent #GNode to use..
 * @api_version: the AppStream API version
 *
 * Inserts the icon into the DOM tree.
 *
 * Returns: (transfer none): A populated #GNode
 *
 * Since: 0.3.1
 **/
GNode *
as_icon_node_insert (AsIcon *icon, GNode *parent, gdouble api_version)
{
	AsIconPrivate *priv = GET_PRIVATE (icon);
	GNode *n;
	_cleanup_free_ gchar *data = NULL;

	/* normal icon */
	if (priv->kind != AS_ICON_KIND_EMBEDDED) {
		n = as_node_insert (parent, "icon", priv->name, 0,
				    "type", as_icon_kind_to_string (priv->kind),
				    NULL);
		if (priv->kind == AS_ICON_KIND_CACHED && api_version >= 0.8) {
			if (priv->width > 0)
				as_node_add_attribute_as_int (n, "width", priv->width);
			if (priv->height > 0)
				as_node_add_attribute_as_int (n, "height", priv->height);
		}
		return n;
	}

	/* embedded icon */
	n = as_node_insert (parent, "icon", NULL, 0,
			    "type", as_icon_kind_to_string (priv->kind),
			    NULL);
	if (api_version >= 0.8) {
		as_node_add_attribute_as_int (n, "width", priv->width);
		as_node_add_attribute_as_int (n, "height", priv->height);
	}
	as_node_insert (n, "name", priv->name, 0, NULL);
	data = g_base64_encode (g_bytes_get_data (priv->data, NULL),
				g_bytes_get_size (priv->data));
	as_node_insert (n, "filecontent", data,
			AS_NODE_INSERT_FLAG_BASE64_ENCODED, NULL);
	return n;
}

/**
 * as_icon_node_parse_embedded:
 **/
static gboolean
as_icon_node_parse_embedded (AsIcon *icon, GNode *n, GError **error)
{
	AsIconPrivate *priv = GET_PRIVATE (icon);
	GNode *c;
	gsize size;
	_cleanup_free_ guchar *data = NULL;
	_cleanup_object_unref_ GdkPixbuf *pixbuf = NULL;
	_cleanup_object_unref_ GInputStream *stream = NULL;

	/* get the icon name */
	c = as_node_find (n, "name");
	if (c == NULL) {
		g_set_error_literal (error,
				     AS_ICON_ERROR,
				     AS_ICON_ERROR_FAILED,
				     "embedded icons needs <name>");
		return FALSE;
	}
	g_free (priv->name);
	priv->name = as_node_take_data (c);

	/* parse the Base64 data */
	c = as_node_find (n, "filecontent");
	if (c == NULL) {
		g_set_error_literal (error,
				     AS_ICON_ERROR,
				     AS_ICON_ERROR_FAILED,
				     "embedded icons needs <filecontent>");
		return FALSE;
	}
	data = g_base64_decode (as_node_get_data (c), &size);
	stream = g_memory_input_stream_new_from_data (data, (gssize) size, NULL);
	if (stream == NULL) {
		g_set_error_literal (error,
				     AS_ICON_ERROR,
				     AS_ICON_ERROR_FAILED,
				     "failed to load embedded data");
		return FALSE;
	}

	/* load the image */
	pixbuf = gdk_pixbuf_new_from_stream (stream, NULL, error);
	if (pixbuf == NULL)
		return FALSE;
	as_icon_set_pixbuf (icon, pixbuf);

	/* save the raw data */
	if (priv->data != NULL)
		g_bytes_unref (priv->data);
	priv->data = g_bytes_new (data, size);

	return TRUE;
}

/**
 * as_icon_node_parse:
 * @icon: a #AsIcon instance.
 * @node: a #GNode.
 * @error: A #GError or %NULL.
 *
 * Populates the object from a DOM node.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.3.1
 **/
gboolean
as_icon_node_parse (AsIcon *icon, GNode *node, GError **error)
{
	AsIconPrivate *priv = GET_PRIVATE (icon);
	const gchar *tmp;
	gint size;
	gboolean prepend_size = TRUE;

	tmp = as_node_get_attribute (node, "type");
	as_icon_set_kind (icon, as_icon_kind_from_string (tmp));
	switch (priv->kind) {
	case AS_ICON_KIND_EMBEDDED:
		if (!as_icon_node_parse_embedded (icon, node, error))
			return FALSE;
		break;
	default:

		/* store the name without any prefix */
		tmp = as_node_get_data (node);
		if (g_strstr_len (tmp, -1, "/") == NULL) {
			as_icon_set_name (icon, tmp, -1);
		} else {
			_cleanup_free_ gchar *basename = NULL;
			basename = g_path_get_basename (tmp);
			as_icon_set_name (icon, basename, -1);
		}

		/* width is optional, assume 64px if missing */
		size = as_node_get_attribute_as_int (node, "width");
		if (size == G_MAXINT) {
			size = 64;
			prepend_size = FALSE;
		}
		priv->width = size;

		/* height is optional, assume 64px if missing */
		size = as_node_get_attribute_as_int (node, "height");
		if (size == G_MAXINT) {
			size = 64;
			prepend_size = FALSE;
		}
		priv->height = size;

		/* only use the size if the metadata has width and height */
		if (prepend_size) {
			g_free (priv->prefix_private);
			priv->prefix_private = g_strdup_printf ("%s/%ix%i",
								priv->prefix,
								priv->width,
								priv->height);
		}
		break;
	}

	return TRUE;
}

/**
 * as_icon_node_parse_dep11:
 * @icon: a #AsIcon instance.
 * @node: a #GNode.
 * @error: A #GError or %NULL.
 *
 * Populates the object from a DEP-11 node.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.3.1
 **/
gboolean
as_icon_node_parse_dep11 (AsIcon *im, GNode *node, GError **error)
{
	if (g_strcmp0 (as_yaml_node_get_key (node), "cached") != 0)
		return TRUE;
	as_icon_set_name (im, as_yaml_node_get_value (node), -1);
	as_icon_set_kind (im, AS_ICON_KIND_CACHED);
	return TRUE;
}

/**
 * as_icon_load:
 * @icon: a #AsIcon instance.
 * @flags: a #AsIconLoadFlags, e.g. %AS_ICON_LOAD_FLAG_SEARCH_SIZE
 * @error: A #GError or %NULL.
 *
 * Loads the icon into a local pixbuf.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.3.1
 **/
gboolean
as_icon_load (AsIcon *icon, AsIconLoadFlags flags, GError **error)
{
	AsIconPrivate *priv = GET_PRIVATE (icon);
	_cleanup_free_ gchar *fn_fallback = NULL;
	_cleanup_free_ gchar *fn_size = NULL;
	_cleanup_free_ gchar *size_str = NULL;
	_cleanup_object_unref_ GdkPixbuf *pixbuf = NULL;

	/* absolute filename */
	if (priv->kind == AS_ICON_KIND_LOCAL) {
		pixbuf = gdk_pixbuf_new_from_file (priv->name, error);
		if (pixbuf == NULL)
			return FALSE;
		as_icon_set_pixbuf (icon, pixbuf);
		return TRUE;
	}

	/* not set */
	if (priv->prefix == NULL) {
		g_set_error (error,
			     AS_ICON_ERROR,
			     AS_ICON_ERROR_FAILED,
			     "unable to load '%s' as no prefix set",
			     priv->name);
		return FALSE;
	}

	/* try getting a pixbuf of the right size */
	if (flags & AS_ICON_LOAD_FLAG_SEARCH_SIZE) {
		size_str = g_strdup_printf ("%ix%i", priv->width, priv->height);
		fn_size = g_build_filename (priv->prefix, size_str, priv->name, NULL);
		if (g_file_test (fn_size, G_FILE_TEST_EXISTS)) {
			pixbuf = gdk_pixbuf_new_from_file (fn_size, error);
			if (pixbuf == NULL)
				return FALSE;
			as_icon_set_pixbuf (icon, pixbuf);
			return TRUE;
		}
	}

	/* fall back to the old location */
	fn_fallback = g_build_filename (priv->prefix, priv->name, NULL);
	pixbuf = gdk_pixbuf_new_from_file (fn_fallback, error);
	if (pixbuf == NULL)
		return FALSE;
	as_icon_set_pixbuf (icon, pixbuf);
	return TRUE;
}

/**
 * as_icon_convert_to_kind:
 * @icon: a #AsIcon instance.
 * @kind: a %AsIconKind, e.g. #AS_ICON_KIND_EMBEDDED
 * @error: A #GError or %NULL.
 *
 * Converts the icon from one kind to another.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.3.1
 **/
gboolean
as_icon_convert_to_kind (AsIcon *icon, AsIconKind kind, GError **error)
{
	AsIconPrivate *priv = GET_PRIVATE (icon);

	/* these can't be converted */
	if (priv->kind == AS_ICON_KIND_STOCK ||
	    priv->kind == AS_ICON_KIND_REMOTE)
		return TRUE;

	/* no change */
	if (priv->kind == kind)
		return TRUE;

	/* cached -> embedded */
	if (priv->kind == AS_ICON_KIND_CACHED && kind == AS_ICON_KIND_EMBEDDED) {
		gsize data_size;
		_cleanup_bytes_unref_ GBytes *tmp = NULL;
		_cleanup_free_ gchar *data = NULL;

		/* load the pixbuf and save it to a PNG buffer */
		if (priv->pixbuf == NULL) {
			if (!as_icon_load (icon, AS_ICON_LOAD_FLAG_SEARCH_SIZE, error))
				return FALSE;
		}
		if (!gdk_pixbuf_save_to_buffer (priv->pixbuf, &data, &data_size,
						"png", error, NULL))
			return FALSE;

		/* set the PNG buffer to a blob of data */
		tmp = g_bytes_new (data, data_size);
		as_icon_set_data (icon, tmp);
		as_icon_set_kind (icon, kind);
		return TRUE;
	}

	/* cached -> embedded */
	if (priv->kind == AS_ICON_KIND_EMBEDDED && kind == AS_ICON_KIND_CACHED) {
		_cleanup_free_ gchar *size_str = NULL;
		_cleanup_free_ gchar *path = NULL;
		_cleanup_free_ gchar *fn = NULL;

		/* ensure the parent path exists */
		size_str = g_strdup_printf ("%ix%i", priv->width, priv->height);
		path = g_build_filename (priv->prefix, size_str, NULL);
		if (g_mkdir_with_parents (path, 0700) != 0) {
			g_set_error (error,
				     AS_ICON_ERROR,
				     AS_ICON_ERROR_FAILED,
				     "Failed to create: %s", path);
			return FALSE;
		}

		/* save the pixbuf */
		fn = g_build_filename (path, priv->name, NULL);
		if (!gdk_pixbuf_save (priv->pixbuf, fn, "png", error, NULL))
			return FALSE;
		as_icon_set_kind (icon, kind);
		return TRUE;
	}

	/* not supported */
	g_set_error (error,
		     AS_ICON_ERROR,
		     AS_ICON_ERROR_FAILED,
		     "converting %s to %s is not supported",
		     as_icon_kind_to_string (priv->kind),
		     as_icon_kind_to_string (kind));
	return FALSE;
}

/**
 * as_icon_new:
 *
 * Creates a new #AsIcon.
 *
 * Returns: (transfer full): a #AsIcon
 *
 * Since: 0.3.1
 **/
AsIcon *
as_icon_new (void)
{
	AsIcon *icon;
	icon = g_object_new (AS_TYPE_ICON, NULL);
	return AS_ICON (icon);
}
