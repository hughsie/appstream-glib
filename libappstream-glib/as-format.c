/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

/**
 * SECTION:as-format
 * @short_description: Object representing where information about a AsApp came from.
 * @include: appstream-glib.h
 * @stability: Stable
 *
 * AsApp's may be made from several information formats, and this object
 * represents the filename (and kind) of the format.
 *
 * See also: #AsApp
 */

#include "config.h"

#include "as-format.h"
#include "as-ref-string.h"

typedef struct
{
	AsFormatKind		 kind;
	AsRefString		*filename;
} AsFormatPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsFormat, as_format, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_format_get_instance_private (o))

static void
as_format_finalize (GObject *object)
{
	AsFormat *format = AS_FORMAT (object);
	AsFormatPrivate *priv = GET_PRIVATE (format);

	if (priv->filename != NULL)
		as_ref_string_unref (priv->filename);

	G_OBJECT_CLASS (as_format_parent_class)->finalize (object);
}

static void
as_format_init (AsFormat *format)
{
}

static void
as_format_class_init (AsFormatClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_format_finalize;
}

/**
 * as_format_kind_from_string:
 * @kind: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: (transfer full): a #AsFormatKind, or %AS_FORMAT_KIND_UNKNOWN for unknown.
 *
 * Since: 0.6.9
 **/
AsFormatKind
as_format_kind_from_string (const gchar *kind)
{
	if (g_strcmp0 (kind, "appstream") == 0)
		return AS_FORMAT_KIND_APPSTREAM;
	if (g_strcmp0 (kind, "appdata") == 0)
		return AS_FORMAT_KIND_APPDATA;
	if (g_strcmp0 (kind, "metainfo") == 0)
		return AS_FORMAT_KIND_METAINFO;
	if (g_strcmp0 (kind, "desktop") == 0)
		return AS_FORMAT_KIND_DESKTOP;
	return AS_FORMAT_KIND_UNKNOWN;
}

/**
 * as_kind_to_string:
 * @kind: the #AsFormatKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @kind
 *
 * Since: 0.6.9
 **/
const gchar *
as_format_kind_to_string (AsFormatKind kind)
{
	if (kind == AS_FORMAT_KIND_APPSTREAM)
		return "appstream";
	if (kind == AS_FORMAT_KIND_APPDATA)
		return "appdata";
	if (kind == AS_FORMAT_KIND_METAINFO)
		return "metainfo";
	if (kind == AS_FORMAT_KIND_DESKTOP)
		return "desktop";
	return NULL;
}

/**
 * as_format_get_filename:
 * @format: a #AsFormat instance.
 *
 * Gets the filename required for this format.
 *
 * Returns: Runtime identifier, e.g. "org.gnome.Platform/i386/master"
 *
 * Since: 0.6.9
 **/
const gchar *
as_format_get_filename (AsFormat *format)
{
	AsFormatPrivate *priv = GET_PRIVATE (format);
	g_return_val_if_fail (AS_IS_FORMAT (format), NULL);
	return priv->filename;
}

/**
 * as_format_get_kind:
 * @format: a #AsFormat instance.
 *
 * Gets the format kind.
 *
 * Returns: the #AsFormatKind
 *
 * Since: 0.6.9
 **/
AsFormatKind
as_format_get_kind (AsFormat *format)
{
	AsFormatPrivate *priv = GET_PRIVATE (format);
	g_return_val_if_fail (AS_IS_FORMAT (format), AS_FORMAT_KIND_UNKNOWN);
	return priv->kind;
}

/**
 * as_format_guess_kind:
 * @filename: a file name
 *
 * Guesses the source kind based from the filename.
 *
 * Return value: A #AsFormatKind, e.g. %AS_FORMAT_KIND_APPSTREAM.
 *
 * Since: 0.6.9
 **/
AsFormatKind
as_format_guess_kind (const gchar *filename)
{
	if (g_str_has_suffix (filename, ".xml.gz"))
		return AS_FORMAT_KIND_APPSTREAM;
	if (g_str_has_suffix (filename, ".yml"))
		return AS_FORMAT_KIND_APPSTREAM;
	if (g_str_has_suffix (filename, ".yml.gz"))
		return AS_FORMAT_KIND_APPSTREAM;
	if (g_str_has_suffix (filename, ".desktop"))
		return AS_FORMAT_KIND_DESKTOP;
	if (g_str_has_suffix (filename, ".desktop.in"))
		return AS_FORMAT_KIND_DESKTOP;
	if (g_str_has_suffix (filename, ".appdata.xml"))
		return AS_FORMAT_KIND_APPDATA;
	if (g_str_has_suffix (filename, ".appdata.xml.in"))
		return AS_FORMAT_KIND_APPDATA;
	if (g_str_has_suffix (filename, ".metainfo.xml"))
		return AS_FORMAT_KIND_METAINFO;
	if (g_str_has_suffix (filename, ".metainfo.xml.in"))
		return AS_FORMAT_KIND_METAINFO;
	if (g_str_has_suffix (filename, ".xml"))
		return AS_FORMAT_KIND_APPSTREAM;
	return AS_FORMAT_KIND_UNKNOWN;
}

/**
 * as_format_set_filename:
 * @format: a #AsFormat instance.
 * @filename: the URL.
 *
 * Sets the filename required for this format.
 *
 * Since: 0.6.9
 **/
void
as_format_set_filename (AsFormat *format, const gchar *filename)
{
	AsFormatPrivate *priv = GET_PRIVATE (format);
	g_return_if_fail (AS_IS_FORMAT (format));
	if (priv->kind == AS_FORMAT_KIND_UNKNOWN)
		priv->kind = as_format_guess_kind (filename);
	as_ref_string_assign_safe (&priv->filename, filename);
}

/**
 * as_format_set_kind:
 * @format: a #AsFormat instance.
 * @kind: the #AsFormatKind, e.g. %AS_FORMAT_KIND_APPDATA.
 *
 * Sets the format kind.
 *
 * Since: 0.6.9
 **/
void
as_format_set_kind (AsFormat *format, AsFormatKind kind)
{
	AsFormatPrivate *priv = GET_PRIVATE (format);
	g_return_if_fail (AS_IS_FORMAT (format));
	priv->kind = kind;
}

/**
 * as_format_equal:
 * @format1: a #AsFormat instance.
 * @format2: a #AsFormat instance.
 *
 * Checks if two formats are the same.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.6.9
 **/
gboolean
as_format_equal (AsFormat *format1, AsFormat *format2)
{
	AsFormatPrivate *priv1 = GET_PRIVATE (format1);
	AsFormatPrivate *priv2 = GET_PRIVATE (format2);

	g_return_val_if_fail (AS_IS_FORMAT (format1), FALSE);
	g_return_val_if_fail (AS_IS_FORMAT (format2), FALSE);

	/* trivial */
	if (format1 == format2)
		return TRUE;

	/* check for equality */
	if (priv1->kind != priv2->kind)
		return FALSE;
	if (g_strcmp0 (priv1->filename, priv2->filename) != 0)
		return FALSE;

	/* success */
	return TRUE;
}

/**
 * as_format_new:
 *
 * Creates a new #AsFormat.
 *
 * Returns: (transfer full): a #AsFormat
 *
 * Since: 0.6.9
 **/
AsFormat *
as_format_new (void)
{
	AsFormat *format;
	format = g_object_new (AS_TYPE_FORMAT, NULL);
	return AS_FORMAT (format);
}
