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
 * SECTION:as-provide
 * @short_description: Object representing a single object the application provides.
 * @include: appstream-glib.h
 * @stability: Stable
 *
 * Applications may provide different binary names, firmware files and that
 * kind of thing. This is the place to express those extra items.
 *
 * See also: #AsApp
 */

#include "config.h"

#include "as-node-private.h"
#include "as-provide-private.h"
#include "as-ref-string.h"
#include "as-utils-private.h"
#include "as-yaml.h"

typedef struct
{
	AsProvideKind		 kind;
	AsRefString		*value;
} AsProvidePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsProvide, as_provide, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_provide_get_instance_private (o))

static void
as_provide_finalize (GObject *object)
{
	AsProvide *provide = AS_PROVIDE (object);
	AsProvidePrivate *priv = GET_PRIVATE (provide);

	if (priv->value != NULL)
		as_ref_string_unref (priv->value);

	G_OBJECT_CLASS (as_provide_parent_class)->finalize (object);
}

static void
as_provide_init (AsProvide *provide)
{
}

static void
as_provide_class_init (AsProvideClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_provide_finalize;
}


/**
 * as_provide_kind_from_string:
 * @kind: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: (transfer full): a #AsProvideKind, or %AS_PROVIDE_KIND_UNKNOWN for unknown.
 *
 * Since: 0.1.6
 **/
AsProvideKind
as_provide_kind_from_string (const gchar *kind)
{
	if (g_strcmp0 (kind, "library") == 0)
		return AS_PROVIDE_KIND_LIBRARY;
	if (g_strcmp0 (kind, "binary") == 0)
		return AS_PROVIDE_KIND_BINARY;
	if (g_strcmp0 (kind, "font") == 0)
		return AS_PROVIDE_KIND_FONT;
	if (g_strcmp0 (kind, "modalias") == 0)
		return AS_PROVIDE_KIND_MODALIAS;
	if (g_strcmp0 (kind, "firmware-runtime") == 0)
		return AS_PROVIDE_KIND_FIRMWARE_RUNTIME;
	if (g_strcmp0 (kind, "firmware-flashed") == 0)
		return AS_PROVIDE_KIND_FIRMWARE_FLASHED;
	if (g_strcmp0 (kind, "python2") == 0)
		return AS_PROVIDE_KIND_PYTHON2;
	if (g_strcmp0 (kind, "python3") == 0)
		return AS_PROVIDE_KIND_PYTHON3;
	if (g_strcmp0 (kind, "dbus-session") == 0)
		return AS_PROVIDE_KIND_DBUS_SESSION;
	if (g_strcmp0 (kind, "dbus-system") == 0)
		return AS_PROVIDE_KIND_DBUS_SYSTEM;
	return AS_PROVIDE_KIND_UNKNOWN;
}

/**
 * as_provide_kind_to_string:
 * @kind: the #AsProvideKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @kind
 *
 * Since: 0.1.6
 **/
const gchar *
as_provide_kind_to_string (AsProvideKind kind)
{
	if (kind == AS_PROVIDE_KIND_LIBRARY)
		return "library";
	if (kind == AS_PROVIDE_KIND_BINARY)
		return "binary";
	if (kind == AS_PROVIDE_KIND_FONT)
		return "font";
	if (kind == AS_PROVIDE_KIND_MODALIAS)
		return "modalias";
	if (kind == AS_PROVIDE_KIND_FIRMWARE_RUNTIME)
		return "firmware-runtime";
	if (kind == AS_PROVIDE_KIND_FIRMWARE_FLASHED)
		return "firmware-flashed";
	if (kind == AS_PROVIDE_KIND_PYTHON2)
		return "python2";
	if (kind == AS_PROVIDE_KIND_PYTHON3)
		return "python3";
	if (kind == AS_PROVIDE_KIND_DBUS_SESSION)
		return "dbus";
	if (kind == AS_PROVIDE_KIND_DBUS_SYSTEM)
		return "dbus-system";
	return NULL;
}

/**
 * as_provide_get_value:
 * @provide: a #AsProvide instance.
 *
 * Gets the full qualified URL for the provide, usually pointing at some mirror.
 *
 * Returns: URL
 *
 * Since: 0.1.6
 **/
const gchar *
as_provide_get_value (AsProvide *provide)
{
	AsProvidePrivate *priv = GET_PRIVATE (provide);
	return priv->value;
}

/**
 * as_provide_get_kind:
 * @provide: a #AsProvide instance.
 *
 * Gets the provide kind.
 *
 * Returns: the #AsProvideKind
 *
 * Since: 0.1.6
 **/
AsProvideKind
as_provide_get_kind (AsProvide *provide)
{
	AsProvidePrivate *priv = GET_PRIVATE (provide);
	return priv->kind;
}

/**
 * as_provide_set_value:
 * @provide: a #AsProvide instance.
 * @value: the URL.
 *
 * Sets the fully-qualified mirror URL to use for the provide.
 *
 * Since: 0.1.6
 **/
void
as_provide_set_value (AsProvide *provide, const gchar *value)
{
	AsProvidePrivate *priv = GET_PRIVATE (provide);
	as_ref_string_assign_safe (&priv->value, value);
}

/**
 * as_provide_set_kind:
 * @provide: a #AsProvide instance.
 * @kind: the #AsProvideKind, e.g. %AS_PROVIDE_KIND_LIBRARY.
 *
 * Sets the provide kind.
 *
 * Since: 0.1.6
 **/
void
as_provide_set_kind (AsProvide *provide, AsProvideKind kind)
{
	AsProvidePrivate *priv = GET_PRIVATE (provide);
	priv->kind = kind;
}

/**
 * as_provide_node_insert: (skip)
 * @provide: a #AsProvide instance.
 * @parent: the parent #GNode to use..
 * @ctx: the #AsNodeContext
 *
 * Inserts the provide into the DOM tree.
 *
 * Returns: (transfer none): A populated #GNode
 *
 * Since: 0.1.6
 **/
GNode *
as_provide_node_insert (AsProvide *provide, GNode *parent, AsNodeContext *ctx)
{
	AsProvidePrivate *priv = GET_PRIVATE (provide);
	GNode *n = NULL;

	switch (priv->kind) {
	case AS_PROVIDE_KIND_UNKNOWN:
		break;
	case AS_PROVIDE_KIND_DBUS_SESSION:
		n = as_node_insert (parent, "dbus",
				    priv->value,
				    AS_NODE_INSERT_FLAG_NONE,
				    "type", "session",
				    NULL);
		break;
	case AS_PROVIDE_KIND_DBUS_SYSTEM:
		n = as_node_insert (parent, "dbus",
				    priv->value,
				    AS_NODE_INSERT_FLAG_NONE,
				    "type", "system",
				    NULL);
		break;
	case AS_PROVIDE_KIND_FIRMWARE_FLASHED:
		n = as_node_insert (parent, "firmware",
				    priv->value,
				    AS_NODE_INSERT_FLAG_NONE,
				    "type", "flashed",
				    NULL);
		break;
	case AS_PROVIDE_KIND_FIRMWARE_RUNTIME:
		n = as_node_insert (parent, "firmware",
				    priv->value,
				    AS_NODE_INSERT_FLAG_NONE,
				    "type", "runtime",
				    NULL);
		break;
	default:
		n = as_node_insert (parent, as_provide_kind_to_string (priv->kind),
				    priv->value,
				    AS_NODE_INSERT_FLAG_NONE,
				    NULL);
		break;
	}
	return n;
}

/**
 * as_provide_node_parse_dep11:
 * @provide: a #AsProvide instance.
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
as_provide_node_parse_dep11 (AsProvide *provide, GNode *node,
			     AsNodeContext *ctx, GError **error)
{
	return TRUE;
}

/**
 * as_provide_node_parse:
 * @provide: a #AsProvide instance.
 * @node: a #GNode.
 * @ctx: a #AsNodeContext.
 * @error: A #GError or %NULL.
 *
 * Populates the object from a DOM node.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.1.6
 **/
gboolean
as_provide_node_parse (AsProvide *provide, GNode *node,
		       AsNodeContext *ctx, GError **error)
{
	AsProvidePrivate *priv = GET_PRIVATE (provide);
	const gchar *tmp;

	if (g_strcmp0 (as_node_get_name (node), "dbus") == 0) {
		tmp = as_node_get_attribute (node, "type");
		if (g_strcmp0 (tmp, "system") == 0)
			priv->kind = AS_PROVIDE_KIND_DBUS_SYSTEM;
		else
			priv->kind = AS_PROVIDE_KIND_DBUS_SESSION;
	} else if (g_strcmp0 (as_node_get_name (node), "firmware") == 0) {
		tmp = as_node_get_attribute (node, "type");
		if (g_strcmp0 (tmp, "flashed") == 0)
			priv->kind = AS_PROVIDE_KIND_FIRMWARE_FLASHED;
		else
			priv->kind = AS_PROVIDE_KIND_FIRMWARE_RUNTIME;
	} else {
		priv->kind = as_provide_kind_from_string (as_node_get_name (node));
	}
	as_ref_string_assign (&priv->value, as_node_get_data (node));
	return TRUE;
}

/**
 * as_provide_new:
 *
 * Creates a new #AsProvide.
 *
 * Returns: (transfer full): a #AsProvide
 *
 * Since: 0.1.6
 **/
AsProvide *
as_provide_new (void)
{
	AsProvide *provide;
	provide = g_object_new (AS_TYPE_PROVIDE, NULL);
	return AS_PROVIDE (provide);
}
