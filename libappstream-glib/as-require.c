/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016 Richard Hughes <richard@hughsie.com>
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
 * SECTION:as-require
 * @short_description: Object representing a single requirement.
 * @include: appstream-glib.h
 * @stability: Unstable
 *
 * Requirements are things the component needs to be valid.
 *
 * See also: #AsApp
 */

#include "config.h"

#include "as-require-private.h"
#include "as-node-private.h"
#include "as-ref-string.h"
#include "as-utils-private.h"

typedef struct
{
	AsRequireKind		 kind;
	AsRequireCompare	 compare;
	AsRefString		*version;	/* utf8 */
	AsRefString		*value;	/* utf8 */
} AsRequirePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsRequire, as_require, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_require_get_instance_private (o))

static void
as_require_finalize (GObject *object)
{
	AsRequire *require = AS_REQUIRE (object);
	AsRequirePrivate *priv = GET_PRIVATE (require);

	if (priv->version != NULL)
		as_ref_string_unref (priv->version);
	if (priv->value != NULL)
		as_ref_string_unref (priv->value);

	G_OBJECT_CLASS (as_require_parent_class)->finalize (object);
}

static void
as_require_init (AsRequire *require)
{
//	AsRequirePrivate *priv = GET_PRIVATE (require);
}

static void
as_require_class_init (AsRequireClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_require_finalize;
}

/**
 * as_require_kind_from_string:
 * @kind: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: (transfer full): a #AsRequireKind, or %AS_REQUIRE_KIND_UNKNOWN for unknown.
 *
 * Since: 0.6.7
 **/
AsRequireKind
as_require_kind_from_string (const gchar *kind)
{
	if (g_strcmp0 (kind, "id") == 0)
		return AS_REQUIRE_KIND_ID;
	if (g_strcmp0 (kind, "firmware") == 0)
		return AS_REQUIRE_KIND_FIRMWARE;
	return AS_REQUIRE_KIND_UNKNOWN;
}

/**
 * as_require_kind_to_string:
 * @kind: the #AsRequireKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @kind
 *
 * Since: 0.6.7
 **/
const gchar *
as_require_kind_to_string (AsRequireKind kind)
{
	if (kind == AS_REQUIRE_KIND_ID)
		return "id";
	if (kind == AS_REQUIRE_KIND_FIRMWARE)
		return "firmware";
	return NULL;
}

/**
 * as_require_compare_from_string:
 * @compare: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: (transfer full): a #AsRequireCompare, or
 *			     %AS_REQUIRE_COMPARE_UNKNOWN for unknown.
 *
 * Since: 0.6.7
 **/
AsRequireCompare
as_require_compare_from_string (const gchar *compare)
{
	if (g_strcmp0 (compare, "eq") == 0)
		return AS_REQUIRE_COMPARE_EQ;
	if (g_strcmp0 (compare, "ne") == 0)
		return AS_REQUIRE_COMPARE_NE;
	if (g_strcmp0 (compare, "gt") == 0)
		return AS_REQUIRE_COMPARE_GT;
	if (g_strcmp0 (compare, "lt") == 0)
		return AS_REQUIRE_COMPARE_LT;
	if (g_strcmp0 (compare, "ge") == 0)
		return AS_REQUIRE_COMPARE_GE;
	if (g_strcmp0 (compare, "le") == 0)
		return AS_REQUIRE_COMPARE_LE;
	return AS_REQUIRE_COMPARE_UNKNOWN;
}

/**
 * as_require_compare_to_string:
 * @compare: the #AsRequireCompare.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @compare
 *
 * Since: 0.6.7
 **/
const gchar *
as_require_compare_to_string (AsRequireCompare compare)
{
	if (compare == AS_REQUIRE_COMPARE_EQ)
		return "eq";
	if (compare == AS_REQUIRE_COMPARE_NE)
		return "ne";
	if (compare == AS_REQUIRE_COMPARE_GT)
		return "gt";
	if (compare == AS_REQUIRE_COMPARE_LT)
		return "lt";
	if (compare == AS_REQUIRE_COMPARE_GE)
		return "ge";
	if (compare == AS_REQUIRE_COMPARE_LE)
		return "le";
	return NULL;
}

/**
 * as_require_get_version:
 * @require: a #AsRequire instance.
 *
 * Gets the require version if set.
 *
 * Returns: the version, e.g. "0.1.2"
 *
 * Since: 0.6.7
 **/
const gchar *
as_require_get_version (AsRequire *require)
{
	AsRequirePrivate *priv = GET_PRIVATE (require);
	return priv->version;
}

/**
 * as_require_get_value:
 * @require: a #AsRequire instance.
 *
 * Gets the require value if set.
 *
 * Returns: the value, e.g. "bootloader"
 *
 * Since: 0.6.7
 **/
const gchar *
as_require_get_value (AsRequire *require)
{
	AsRequirePrivate *priv = GET_PRIVATE (require);
	return priv->value;
}

/**
 * as_require_get_kind:
 * @require: a #AsRequire instance.
 *
 * Gets the require kind.
 *
 * Returns: the #AsRequireKind
 *
 * Since: 0.6.7
 **/
AsRequireKind
as_require_get_kind (AsRequire *require)
{
	AsRequirePrivate *priv = GET_PRIVATE (require);
	return priv->kind;
}

/**
 * as_require_set_kind:
 * @require: a #AsRequire instance.
 * @kind: the #AsRequireKind, e.g. %AS_REQUIRE_KIND_ID.
 *
 * Sets the require kind.
 *
 * Since: 0.6.7
 **/
void
as_require_set_kind (AsRequire *require, AsRequireKind kind)
{
	AsRequirePrivate *priv = GET_PRIVATE (require);
	priv->kind = kind;
}

/**
 * as_require_get_compare:
 * @require: a #AsRequire instance.
 *
 * Gets the require version comparison type.
 *
 * Returns: the #AsRequireKind
 *
 * Since: 0.6.7
 **/
AsRequireCompare
as_require_get_compare (AsRequire *require)
{
	AsRequirePrivate *priv = GET_PRIVATE (require);
	return priv->compare;
}

/**
 * as_require_set_compare:
 * @require: a #AsRequire instance.
 * @compare: the #AsRequireKind, e.g. %AS_REQUIRE_KIND_ID.
 *
 * Sets the require version comparison type.
 *
 * Since: 0.6.7
 **/
void
as_require_set_compare (AsRequire *require, AsRequireCompare compare)
{
	AsRequirePrivate *priv = GET_PRIVATE (require);
	priv->compare = compare;
}

/**
 * as_require_set_version:
 * @require: a #AsRequire instance.
 * @version: an version number, e.g. `0.1.2`
 *
 * Sets the require version.
 *
 * Since: 0.6.7
 **/
void
as_require_set_version (AsRequire *require, const gchar *version)
{
	AsRequirePrivate *priv = GET_PRIVATE (require);
	if (priv->version != NULL)
		as_ref_string_unref (priv->version);
	priv->version = as_ref_string_new (version);
}

/**
 * as_require_set_value:
 * @require: a #AsRequire instance.
 * @value: an require version, e.g. `firmware`
 *
 * Sets the require value.
 *
 * Since: 0.6.7
 **/
void
as_require_set_value (AsRequire *require, const gchar *value)
{
	AsRequirePrivate *priv = GET_PRIVATE (require);
	if (priv->value != NULL)
		as_ref_string_unref (priv->value);
	priv->value = as_ref_string_new (value);
}


/**
 * as_require_version_compare:
 * @require: a #AsRequire instance.
 * @version: a version number, e.g. `0.1.3`
 * @error: A #GError or %NULL
 *
 * Compares the version number of the requirement with a predicate.
 *
 * Returns: %TRUE if the predicate was true
 *
 * Since: 0.6.7
 **/
gboolean
as_require_version_compare (AsRequire *require,
			    const gchar *version,
			    GError **error)
{
	AsRequirePrivate *priv = GET_PRIVATE (require);
	gint tmp = as_utils_vercmp (version, priv->version);
	gboolean ret = FALSE;

	switch (priv->compare) {
	case AS_REQUIRE_COMPARE_EQ:
		ret = tmp == 0;
		break;
	case AS_REQUIRE_COMPARE_NE:
		ret = tmp != 0;
		break;
	case AS_REQUIRE_COMPARE_LT:
		ret = tmp < 0;
		break;
	case AS_REQUIRE_COMPARE_GT:
		ret = tmp > 0;
		break;
	case AS_REQUIRE_COMPARE_LE:
		ret = tmp <= 0;
		break;
	case AS_REQUIRE_COMPARE_GE:
		ret = tmp >= 0;
		break;
	default:
		break;
	}

	/* set error */
	if (!ret && error != NULL) {
		g_set_error (error,
			     AS_UTILS_ERROR,
			     AS_UTILS_ERROR_FAILED,
			     "failed predicate [%s %s %s]",
			     priv->version,
			     as_require_compare_to_string (priv->compare),
			     version);
	}
	return ret;
}

/**
 * as_require_node_insert: (skip)
 * @require: a #AsRequire instance.
 * @parent: the parent #GNode to use..
 * @ctx: the #AsNodeContext
 *
 * Inserts the require into the DOM tree.
 *
 * Returns: (transfer none): A populated #GNode
 *
 * Since: 0.6.7
 **/
GNode *
as_require_node_insert (AsRequire *require, GNode *parent, AsNodeContext *ctx)
{
	AsRequirePrivate *priv = GET_PRIVATE (require);
	GNode *n;

	/* don't know what to do here */
	if (priv->kind == AS_REQUIRE_KIND_UNKNOWN)
		return NULL;

	n = as_node_insert (parent, as_require_kind_to_string (priv->kind), NULL,
			    AS_NODE_INSERT_FLAG_NONE,
			    NULL);
	if (priv->compare != AS_REQUIRE_COMPARE_UNKNOWN) {
		as_node_add_attribute (n, "compare",
				       as_require_compare_to_string (priv->compare));
	}
	if (priv->version != NULL)
		as_node_add_attribute (n, "version", priv->version);
	if (priv->value != NULL)
		as_node_set_data (n, priv->value, AS_NODE_INSERT_FLAG_NONE);
	return n;
}

/**
 * as_require_node_parse:
 * @require: a #AsRequire instance.
 * @node: a #GNode.
 * @ctx: a #AsNodeContext.
 * @error: A #GError or %NULL.
 *
 * Populates the object from a DOM node.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.6.7
 **/
gboolean
as_require_node_parse (AsRequire *require, GNode *node,
		       AsNodeContext *ctx, GError **error)
{
	AsRequirePrivate *priv = GET_PRIVATE (require);
	const gchar *tmp;
	tmp = as_node_get_name (node);
	if (tmp != NULL)
		as_require_set_kind (require, as_require_kind_from_string (tmp));
	tmp = as_node_get_attribute (node, "compare");
	if (tmp != NULL)
		as_require_set_compare (require, as_require_compare_from_string (tmp));
	as_ref_string_assign (&priv->version, as_node_get_attribute (node, "version"));
	as_ref_string_assign (&priv->value, as_node_get_data (node));
	return TRUE;
}

/**
 * as_require_node_parse_dep11:
 * @require: a #AsRequire instance.
 * @node: a #GNode.
 * @ctx: a #AsNodeContext.
 * @error: A #GError or %NULL.
 *
 * Populates the object from a DEP-11 node.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.6.7
 **/
gboolean
as_require_node_parse_dep11 (AsRequire *im, GNode *node,
			     AsNodeContext *ctx, GError **error)
{
	return TRUE;
}

/**
 * as_require_new:
 *
 * Creates a new #AsRequire.
 *
 * Returns: (transfer full): a #AsRequire
 *
 * Since: 0.6.7
 **/
AsRequire *
as_require_new (void)
{
	AsRequire *require;
	require = g_object_new (AS_TYPE_REQUIRE, NULL);
	return AS_REQUIRE (require);
}
