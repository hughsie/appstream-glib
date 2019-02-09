/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
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

#include <fnmatch.h>

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
	if (g_strcmp0 (kind, "hardware") == 0)
		return AS_REQUIRE_KIND_HARDWARE;
	if (g_strcmp0 (kind, "modalias") == 0)
		return AS_REQUIRE_KIND_MODALIAS;
	if (g_strcmp0 (kind, "kernel") == 0)
		return AS_REQUIRE_KIND_KERNEL;
	if (g_strcmp0 (kind, "memory") == 0)
		return AS_REQUIRE_KIND_MEMORY;
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
	if (kind == AS_REQUIRE_KIND_HARDWARE)
		return "hardware";
	if (kind == AS_REQUIRE_KIND_MODALIAS)
		return "modalias";
	if (kind == AS_REQUIRE_KIND_KERNEL)
		return "kernel";
	if (kind == AS_REQUIRE_KIND_MEMORY)
		return "memory";
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
	if (g_strcmp0 (compare, "glob") == 0)
		return AS_REQUIRE_COMPARE_GLOB;
	if (g_strcmp0 (compare, "regex") == 0)
		return AS_REQUIRE_COMPARE_REGEX;
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
	if (compare == AS_REQUIRE_COMPARE_GLOB)
		return "glob";
	if (compare == AS_REQUIRE_COMPARE_REGEX)
		return "regex";
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
	g_return_val_if_fail (AS_IS_REQUIRE (require), NULL);
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
	g_return_val_if_fail (AS_IS_REQUIRE (require), NULL);
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
	g_return_val_if_fail (AS_IS_REQUIRE (require), AS_REQUIRE_KIND_UNKNOWN);
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
	g_return_if_fail (AS_IS_REQUIRE (require));
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
	g_return_val_if_fail (AS_IS_REQUIRE (require), AS_REQUIRE_COMPARE_UNKNOWN);
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
	g_return_if_fail (AS_IS_REQUIRE (require));
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
	g_return_if_fail (AS_IS_REQUIRE (require));
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
	g_return_if_fail (AS_IS_REQUIRE (require));
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
	gboolean ret = FALSE;
	gint rc = 0;

	g_return_val_if_fail (AS_IS_REQUIRE (require), FALSE);

	switch (priv->compare) {
	case AS_REQUIRE_COMPARE_EQ:
		rc = as_utils_vercmp_full (version, priv->version, AS_VERSION_COMPARE_FLAG_NONE);
		ret = rc == 0;
		break;
	case AS_REQUIRE_COMPARE_NE:
		rc = as_utils_vercmp_full (version, priv->version, AS_VERSION_COMPARE_FLAG_NONE);
		ret = rc != 0;
		break;
	case AS_REQUIRE_COMPARE_LT:
		rc = as_utils_vercmp_full (version, priv->version, AS_VERSION_COMPARE_FLAG_NONE);
		ret = rc < 0;
		break;
	case AS_REQUIRE_COMPARE_GT:
		rc = as_utils_vercmp_full (version, priv->version, AS_VERSION_COMPARE_FLAG_NONE);
		ret = rc > 0;
		break;
	case AS_REQUIRE_COMPARE_LE:
		rc = as_utils_vercmp_full (version, priv->version, AS_VERSION_COMPARE_FLAG_NONE);
		ret = rc <= 0;
		break;
	case AS_REQUIRE_COMPARE_GE:
		rc = as_utils_vercmp_full (version, priv->version, AS_VERSION_COMPARE_FLAG_NONE);
		ret = rc >= 0;
		break;
	case AS_REQUIRE_COMPARE_GLOB:
		ret = fnmatch (priv->version, version, 0) == 0;
		break;
	case AS_REQUIRE_COMPARE_REGEX:
		ret = g_regex_match_simple (priv->version, version, 0, 0);
		break;
	default:
		break;
	}

	/* could not compare */
	if (rc == G_MAXINT) {
		g_set_error (error,
			     AS_UTILS_ERROR,
			     AS_UTILS_ERROR_FAILED,
			     "failed to compare [%s] and [%s]",
			     priv->version,
			     version);
		return FALSE;
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
 * as_require_equal:
 * @require1: a #AsRequire instance.
 * @require2: a #AsRequire instance.
 *
 * Checks if two requires are the same.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.7.7
 **/
gboolean
as_require_equal (AsRequire *require1, AsRequire *require2)
{
	AsRequirePrivate *priv1 = GET_PRIVATE (require1);
	AsRequirePrivate *priv2 = GET_PRIVATE (require2);

	g_return_val_if_fail (AS_IS_REQUIRE (require1), FALSE);
	g_return_val_if_fail (AS_IS_REQUIRE (require2), FALSE);

	/* trivial */
	if (require1 == require2)
		return TRUE;

	/* check for equality */
	if (priv1->kind != priv2->kind)
		return FALSE;
	if (priv1->compare != priv2->compare)
		return FALSE;
	if (g_strcmp0 (priv1->version, priv2->version) != 0)
		return FALSE;
	if (g_strcmp0 (priv1->value, priv2->value) != 0)
		return FALSE;

	/* success */
	return TRUE;
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

	g_return_val_if_fail (AS_IS_REQUIRE (require), NULL);

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
	g_return_val_if_fail (AS_IS_REQUIRE (require), FALSE);
	tmp = as_node_get_name (node);
	if (tmp != NULL)
		as_require_set_kind (require, as_require_kind_from_string (tmp));
	tmp = as_node_get_attribute (node, "compare");
	if (tmp != NULL)
		as_require_set_compare (require, as_require_compare_from_string (tmp));
	as_ref_string_assign (&priv->version, as_node_get_attribute_as_refstr (node, "version"));
	as_ref_string_assign (&priv->value, as_node_get_data_as_refstr (node));
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
