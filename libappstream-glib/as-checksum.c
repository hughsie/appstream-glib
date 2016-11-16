/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2015 Richard Hughes <richard@hughsie.com>
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
 * SECTION:as-checksum
 * @short_description: Object representing a single checksum used in a release.
 * @include: appstream-glib.h
 * @stability: Stable
 *
 * Checksums are attached to releases.
 *
 * See also: #AsRelease
 */

#include "config.h"

#include "as-checksum-private.h"
#include "as-node-private.h"
#include "as-ref-string.h"
#include "as-utils-private.h"
#include "as-yaml.h"

typedef struct
{
	AsChecksumTarget	 target;
	GChecksumType		 kind;
	AsRefString		*filename;
	AsRefString		*value;
} AsChecksumPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsChecksum, as_checksum, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_checksum_get_instance_private (o))

#define AS_CHECKSUM_UNKNOWN	((guint) -1)

static void
as_checksum_finalize (GObject *object)
{
	AsChecksum *checksum = AS_CHECKSUM (object);
	AsChecksumPrivate *priv = GET_PRIVATE (checksum);

	if (priv->filename != NULL)
		as_ref_string_unref (priv->filename);
	if (priv->value != NULL)
		as_ref_string_unref (priv->value);

	G_OBJECT_CLASS (as_checksum_parent_class)->finalize (object);
}

static void
as_checksum_init (AsChecksum *checksum)
{
	AsChecksumPrivate *priv = GET_PRIVATE (checksum);
	priv->kind = AS_CHECKSUM_UNKNOWN;
}

static void
as_checksum_class_init (AsChecksumClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_checksum_finalize;
}

/**
 * as_checksum_target_from_string:
 * @target: a source kind string
 *
 * Converts the text representation to an enumerated value.
 *
 * Return value: A #AsChecksumTarget, e.g. %AS_CHECKSUM_TARGET_CONTAINER.
 *
 * Since: 0.4.2
 **/
AsChecksumTarget
as_checksum_target_from_string (const gchar *target)
{
	if (g_strcmp0 (target, "container") == 0)
		return AS_CHECKSUM_TARGET_CONTAINER;
	if (g_strcmp0 (target, "content") == 0)
		return AS_CHECKSUM_TARGET_CONTENT;
	return AS_CHECKSUM_TARGET_UNKNOWN;
}

/**
 * as_checksum_target_to_string:
 * @target: the #AsChecksumTarget.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @target, or %NULL for unknown
 *
 * Since: 0.4.2
 **/
const gchar *
as_checksum_target_to_string (AsChecksumTarget target)
{
	if (target == AS_CHECKSUM_TARGET_CONTAINER)
		return "container";
	if (target == AS_CHECKSUM_TARGET_CONTENT)
		return "content";
	return NULL;
}

/**
 * as_checksum_get_filename:
 * @checksum: a #AsChecksum instance.
 *
 * Gets the full qualified URL for the checksum, usually pointing at some mirror.
 *
 * Returns: URL
 *
 * Since: 0.4.2
 **/
const gchar *
as_checksum_get_filename (AsChecksum *checksum)
{
	AsChecksumPrivate *priv = GET_PRIVATE (checksum);
	return priv->filename;
}

/**
 * as_checksum_get_value:
 * @checksum: a #AsChecksum instance.
 *
 * Gets the suggested value the checksum, including file extension.
 *
 * Returns: filename
 *
 * Since: 0.4.2
 **/
const gchar *
as_checksum_get_value (AsChecksum *checksum)
{
	AsChecksumPrivate *priv = GET_PRIVATE (checksum);
	return priv->value;
}

/**
 * as_checksum_get_kind:
 * @checksum: a #AsChecksum instance.
 *
 * Gets the checksum kind.
 *
 * Returns: the #GChecksumType
 *
 * Since: 0.4.2
 **/
GChecksumType
as_checksum_get_kind (AsChecksum *checksum)
{
	AsChecksumPrivate *priv = GET_PRIVATE (checksum);
	return priv->kind;
}

/**
 * as_checksum_get_target:
 * @checksum: a #AsChecksum instance.
 *
 * Gets the checksum target.
 *
 * Returns: the #GChecksumType
 *
 * Since: 0.4.2
 **/
AsChecksumTarget
as_checksum_get_target (AsChecksum *checksum)
{
	AsChecksumPrivate *priv = GET_PRIVATE (checksum);
	return priv->target;
}

/**
 * as_checksum_set_filename:
 * @checksum: a #AsChecksum instance.
 * @filename: the URL.
 *
 * Sets the filename used to generate the checksum.
 *
 * Since: 0.4.2
 **/
void
as_checksum_set_filename (AsChecksum *checksum,
			  const gchar *filename)
{
	AsChecksumPrivate *priv = GET_PRIVATE (checksum);
	as_ref_string_assign_safe (&priv->filename, filename);
}

/**
 * as_checksum_set_value:
 * @checksum: a #AsChecksum instance.
 * @value: the new filename value.
 *
 * Sets the checksum value filename.
 *
 * Since: 0.4.2
 **/
void
as_checksum_set_value (AsChecksum *checksum, const gchar *value)
{
	AsChecksumPrivate *priv = GET_PRIVATE (checksum);
	as_ref_string_assign_safe (&priv->value, value);
}

/**
 * as_checksum_set_kind:
 * @checksum: a #AsChecksum instance.
 * @kind: the #GChecksumType, e.g. %G_CHECKSUM_SHA1.
 *
 * Sets the checksum kind.
 *
 * Since: 0.4.2
 **/
void
as_checksum_set_kind (AsChecksum *checksum, GChecksumType kind)
{
	AsChecksumPrivate *priv = GET_PRIVATE (checksum);
	priv->kind = kind;
}

/**
 * as_checksum_set_target:
 * @checksum: a #AsChecksum instance.
 * @target: the #GChecksumType, e.g. %AS_CHECKSUM_TARGET_CONTAINER.
 *
 * Sets the checksum target.
 *
 * Since: 0.4.2
 **/
void
as_checksum_set_target (AsChecksum *checksum, AsChecksumTarget target)
{
	AsChecksumPrivate *priv = GET_PRIVATE (checksum);
	priv->target = target;
}

static GChecksumType
_g_checksum_type_from_string (const gchar *checksum_type)
{
	if (g_ascii_strcasecmp (checksum_type, "md5") == 0)
		return G_CHECKSUM_MD5;
	if (g_ascii_strcasecmp (checksum_type, "sha1") == 0)
		return G_CHECKSUM_SHA1;
	if (g_ascii_strcasecmp (checksum_type, "sha256") == 0)
		return G_CHECKSUM_SHA256;
	if (g_ascii_strcasecmp (checksum_type, "sha512") == 0)
		return G_CHECKSUM_SHA512;
	return -1;
}

static const gchar *
_g_checksum_type_to_string (GChecksumType checksum_type)
{
	if (checksum_type == G_CHECKSUM_MD5)
		return "md5";
	if (checksum_type == G_CHECKSUM_SHA1)
		return "sha1";
	if (checksum_type == G_CHECKSUM_SHA256)
		return "sha256";
	if (checksum_type == G_CHECKSUM_SHA512)
		return "sha512";
	return NULL;
}

/**
 * as_checksum_node_insert: (skip)
 * @checksum: a #AsChecksum instance.
 * @parent: the parent #GNode to use..
 * @ctx: the #AsNodeContext
 *
 * Inserts the checksum into the DOM tree.
 *
 * Returns: (transfer none): A populated #GNode
 *
 * Since: 0.4.2
 **/
GNode *
as_checksum_node_insert (AsChecksum *checksum, GNode *parent, AsNodeContext *ctx)
{
	AsChecksumPrivate *priv = GET_PRIVATE (checksum);
	GNode *n;

	n = as_node_insert (parent, "checksum", priv->value,
			    AS_NODE_INSERT_FLAG_NONE,
			    NULL);
	if (priv->kind != AS_CHECKSUM_UNKNOWN) {
		as_node_add_attribute (n, "type",
				       _g_checksum_type_to_string (priv->kind));
	}
	if (priv->target != AS_CHECKSUM_TARGET_UNKNOWN) {
		as_node_add_attribute (n, "target",
				       as_checksum_target_to_string (priv->target));
	}
	if (priv->filename != NULL)
		as_node_add_attribute (n, "filename", priv->filename);
	return n;
}

/**
 * as_checksum_node_parse:
 * @checksum: a #AsChecksum instance.
 * @node: a #GNode.
 * @ctx: a #AsNodeContext.
 * @error: A #GError or %NULL.
 *
 * Populates the object from a DOM node.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.4.2
 **/
gboolean
as_checksum_node_parse (AsChecksum *checksum, GNode *node,
			AsNodeContext *ctx, GError **error)
{
	AsChecksumPrivate *priv = GET_PRIVATE (checksum);
	const gchar *tmp;

	tmp = as_node_get_attribute (node, "type");
	if (tmp != NULL)
		priv->kind = _g_checksum_type_from_string (tmp);
	tmp = as_node_get_attribute (node, "target");
	if (tmp != NULL)
		priv->target = as_checksum_target_from_string (tmp);
	as_ref_string_assign (&priv->filename, as_node_get_attribute (node, "filename"));
	as_ref_string_assign (&priv->value, as_node_get_data (node));
	return TRUE;
}

/**
 * as_checksum_node_parse_dep11:
 * @checksum: a #AsChecksum instance.
 * @node: a #GNode.
 * @ctx: a #AsNodeContext.
 * @error: A #GError or %NULL.
 *
 * Populates the object from a DEP-11 node.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.4.2
 **/
gboolean
as_checksum_node_parse_dep11 (AsChecksum *im, GNode *node,
			      AsNodeContext *ctx, GError **error)
{
	GNode *n;
	const gchar *tmp;

	for (n = node->children; n != NULL; n = n->next) {
		tmp = as_yaml_node_get_key (n);
		if (g_strcmp0 (tmp, "sha1") == 0) {
			as_checksum_set_kind (im, G_CHECKSUM_SHA1);
			as_checksum_set_value (im, as_yaml_node_get_value (n));
		} else if (g_strcmp0 (tmp, "sha256") == 0) {
			as_checksum_set_kind (im, G_CHECKSUM_SHA256);
			as_checksum_set_value (im, as_yaml_node_get_value (n));
		} else if (g_strcmp0 (tmp, "md5") == 0) {
			as_checksum_set_kind (im, G_CHECKSUM_MD5);
			as_checksum_set_value (im, as_yaml_node_get_value (n));
		} else if (g_strcmp0 (tmp, "target") == 0) {
			tmp = as_yaml_node_get_value (n);
			as_checksum_set_target (im, as_checksum_target_from_string (tmp));
		} else if (g_strcmp0 (tmp, "filename") == 0) {
			as_checksum_set_filename (im, as_yaml_node_get_value (n));
		}
	}
	return TRUE;
}

/**
 * as_checksum_new:
 *
 * Creates a new #AsChecksum.
 *
 * Returns: (transfer full): a #AsChecksum
 *
 * Since: 0.4.2
 **/
AsChecksum *
as_checksum_new (void)
{
	AsChecksum *checksum;
	checksum = g_object_new (AS_TYPE_CHECKSUM, NULL);
	return AS_CHECKSUM (checksum);
}
