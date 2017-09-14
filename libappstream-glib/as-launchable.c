/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This desktop-id is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This desktop-id is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this desktop-id; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

/**
 * SECTION:as-launchable
 * @short_description: Object representing a way to launch the application.
 * @include: appstream-glib.h
 * @stability: Stable
 *
 * Applications may be launchable using a different application ID to the
 * component ID or may be launchable in some other way, e.g D-Bus, or using
 * the default terminal emulator.
 *
 * See also: #AsApp
 */

#include "config.h"

#include "as-node-private.h"
#include "as-launchable-private.h"
#include "as-ref-string.h"
#include "as-utils-private.h"
#include "as-yaml.h"

typedef struct
{
	AsLaunchableKind	 kind;
	AsRefString		*value;
} AsLaunchablePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsLaunchable, as_launchable, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_launchable_get_instance_private (o))

static void
as_launchable_finalize (GObject *object)
{
	AsLaunchable *launchable = AS_LAUNCHABLE (object);
	AsLaunchablePrivate *priv = GET_PRIVATE (launchable);

	if (priv->value != NULL)
		as_ref_string_unref (priv->value);

	G_OBJECT_CLASS (as_launchable_parent_class)->finalize (object);
}

static void
as_launchable_init (AsLaunchable *launchable)
{
}

static void
as_launchable_class_init (AsLaunchableClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_launchable_finalize;
}


/**
 * as_launchable_kind_from_string:
 * @kind: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: (transfer full): a #AsLaunchableKind, or %AS_LAUNCHABLE_KIND_UNKNOWN for unknown.
 *
 * Since: 0.6.13
 **/
AsLaunchableKind
as_launchable_kind_from_string (const gchar *kind)
{
	if (g_strcmp0 (kind, "desktop-id") == 0)
		return AS_LAUNCHABLE_KIND_DESKTOP_ID;
	if (g_strcmp0 (kind, "service") == 0)
		return AS_LAUNCHABLE_KIND_SERVICE;
	if (g_strcmp0 (kind, "cockpit-manifest") == 0)
		return AS_LAUNCHABLE_KIND_COCKPIT_MANIFEST;
	return AS_LAUNCHABLE_KIND_UNKNOWN;
}

/**
 * as_launchable_kind_to_string:
 * @kind: the #AsLaunchableKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @kind
 *
 * Since: 0.6.13
 **/
const gchar *
as_launchable_kind_to_string (AsLaunchableKind kind)
{
	if (kind == AS_LAUNCHABLE_KIND_DESKTOP_ID)
		return "desktop-id";
	if (kind == AS_LAUNCHABLE_KIND_SERVICE)
		return "service";
	if (kind == AS_LAUNCHABLE_KIND_COCKPIT_MANIFEST)
		return "cockpit-manifest";
	return NULL;
}

/**
 * as_launchable_get_value:
 * @launchable: a #AsLaunchable instance.
 *
 * Gets the value to use for the launchable.
 *
 * Returns: usually a desktop ID, e.g. "gimp.desktop"
 *
 * Since: 0.6.13
 **/
const gchar *
as_launchable_get_value (AsLaunchable *launchable)
{
	AsLaunchablePrivate *priv = GET_PRIVATE (launchable);
	return priv->value;
}

/**
 * as_launchable_get_kind:
 * @launchable: a #AsLaunchable instance.
 *
 * Gets the launchable kind.
 *
 * Returns: the #AsLaunchableKind
 *
 * Since: 0.6.13
 **/
AsLaunchableKind
as_launchable_get_kind (AsLaunchable *launchable)
{
	AsLaunchablePrivate *priv = GET_PRIVATE (launchable);
	return priv->kind;
}

/**
 * as_launchable_set_value:
 * @launchable: a #AsLaunchable instance.
 * @value: the URL.
 *
 * Sets the fully-qualified mirror URL to use for the launchable.
 *
 * Since: 0.6.13
 **/
void
as_launchable_set_value (AsLaunchable *launchable, const gchar *value)
{
	AsLaunchablePrivate *priv = GET_PRIVATE (launchable);
	as_ref_string_assign_safe (&priv->value, value);
}

/**
 * as_launchable_set_kind:
 * @launchable: a #AsLaunchable instance.
 * @kind: the #AsLaunchableKind, e.g. %AS_LAUNCHABLE_KIND_DESKTOP_ID.
 *
 * Sets the launchable kind.
 *
 * Since: 0.6.13
 **/
void
as_launchable_set_kind (AsLaunchable *launchable, AsLaunchableKind kind)
{
	AsLaunchablePrivate *priv = GET_PRIVATE (launchable);
	priv->kind = kind;
}

/**
 * as_launchable_node_insert: (skip)
 * @launchable: a #AsLaunchable instance.
 * @parent: the parent #GNode to use..
 * @ctx: the #AsNodeContext
 *
 * Inserts the launchable into the DOM tree.
 *
 * Returns: (transfer none): A populated #GNode
 *
 * Since: 0.6.13
 **/
GNode *
as_launchable_node_insert (AsLaunchable *launchable, GNode *parent, AsNodeContext *ctx)
{
	AsLaunchablePrivate *priv = GET_PRIVATE (launchable);
	GNode *n = as_node_insert (parent, "launchable",
				   priv->value,
				   AS_NODE_INSERT_FLAG_NONE,
				   NULL);
	if (priv->kind != AS_LAUNCHABLE_KIND_UNKNOWN)
		as_node_add_attribute (n, "type", as_launchable_kind_to_string (priv->kind));
	return n;
}

/**
 * as_launchable_node_parse_dep11:
 * @launchable: a #AsLaunchable instance.
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
as_launchable_node_parse_dep11 (AsLaunchable *launchable, GNode *node,
			        AsNodeContext *ctx, GError **error)
{
	return TRUE;
}

/**
 * as_launchable_node_parse:
 * @launchable: a #AsLaunchable instance.
 * @node: a #GNode.
 * @ctx: a #AsNodeContext.
 * @error: A #GError or %NULL.
 *
 * Populates the object from a DOM node.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.6.13
 **/
gboolean
as_launchable_node_parse (AsLaunchable *launchable, GNode *node,
			  AsNodeContext *ctx, GError **error)
{
	AsLaunchablePrivate *priv = GET_PRIVATE (launchable);
	priv->kind = as_launchable_kind_from_string (as_node_get_attribute (node, "type"));
	as_ref_string_assign (&priv->value, as_node_get_data_as_refstr (node));
	return TRUE;
}

/**
 * as_launchable_new:
 *
 * Creates a new #AsLaunchable.
 *
 * Returns: (transfer full): a #AsLaunchable
 *
 * Since: 0.6.13
 **/
AsLaunchable *
as_launchable_new (void)
{
	AsLaunchable *launchable;
	launchable = g_object_new (AS_TYPE_LAUNCHABLE, NULL);
	return AS_LAUNCHABLE (launchable);
}
