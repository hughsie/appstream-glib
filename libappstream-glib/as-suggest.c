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
 * SECTION:as-suggest
 * @short_description: Object representing a single suggest used in a screenshot.
 * @include: appstream-glib.h
 * @stability: Stable
 *
 * Screenshot may have multiple versions of an suggest in different resolutions
 * or aspect ratios. This object allows access to the location and size of a
 * single suggest.
 *
 * See also: #AsScreenshot
 */

#include "config.h"

#include "as-suggest-private.h"
#include "as-node-private.h"
#include "as-ref-string.h"
#include "as-utils-private.h"

typedef struct
{
	AsSuggestKind		 kind;
	GPtrArray		*ids;	/* utf8 */
} AsSuggestPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsSuggest, as_suggest, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_suggest_get_instance_private (o))

static void
as_suggest_finalize (GObject *object)
{
	AsSuggest *suggest = AS_SUGGEST (object);
	AsSuggestPrivate *priv = GET_PRIVATE (suggest);

	g_ptr_array_unref (priv->ids);

	G_OBJECT_CLASS (as_suggest_parent_class)->finalize (object);
}

static void
as_suggest_init (AsSuggest *suggest)
{
	AsSuggestPrivate *priv = GET_PRIVATE (suggest);
	priv->ids = g_ptr_array_new_with_free_func ((GDestroyNotify) as_ref_string_unref);
}

static void
as_suggest_class_init (AsSuggestClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_suggest_finalize;
}


/**
 * as_suggest_kind_from_string:
 * @kind: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: (transfer full): a #AsSuggestKind, or %AS_SUGGEST_KIND_UNKNOWN for unknown.
 *
 * Since: 0.6.1
 **/
AsSuggestKind
as_suggest_kind_from_string (const gchar *kind)
{
	if (g_strcmp0 (kind, "upstream") == 0)
		return AS_SUGGEST_KIND_UPSTREAM;
	if (g_strcmp0 (kind, "heuristic") == 0)
		return AS_SUGGEST_KIND_HEURISTIC;
	return AS_SUGGEST_KIND_UNKNOWN;
}

/**
 * as_suggest_kind_to_string:
 * @kind: the #AsSuggestKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @kind
 *
 * Since: 0.6.1
 **/
const gchar *
as_suggest_kind_to_string (AsSuggestKind kind)
{
	if (kind == AS_SUGGEST_KIND_UPSTREAM)
		return "upstream";
	if (kind == AS_SUGGEST_KIND_HEURISTIC)
		return "heuristic";
	return NULL;
}

/**
 * as_suggest_get_ids:
 * @suggest: a #AsSuggest instance.
 *
 * Gets the suggest ids if set.
 *
 * Returns: (transfer none) (element-type utf8): the #GPtrArray, or %NULL
 *
 * Since: 0.6.1
 **/
GPtrArray *
as_suggest_get_ids (AsSuggest *suggest)
{
	AsSuggestPrivate *priv = GET_PRIVATE (suggest);
	return priv->ids;
}

/**
 * as_suggest_get_kind:
 * @suggest: a #AsSuggest instance.
 *
 * Gets the suggest kind.
 *
 * Returns: the #AsSuggestKind
 *
 * Since: 0.6.1
 **/
AsSuggestKind
as_suggest_get_kind (AsSuggest *suggest)
{
	AsSuggestPrivate *priv = GET_PRIVATE (suggest);
	return priv->kind;
}

/**
 * as_suggest_set_kind:
 * @suggest: a #AsSuggest instance.
 * @kind: the #AsSuggestKind, e.g. %AS_SUGGEST_KIND_UPSTREAM.
 *
 * Sets the suggest kind.
 *
 * Since: 0.6.1
 **/
void
as_suggest_set_kind (AsSuggest *suggest, AsSuggestKind kind)
{
	AsSuggestPrivate *priv = GET_PRIVATE (suggest);
	priv->kind = kind;
}

/**
 * as_suggest_add_id:
 * @suggest: a #AsSuggest instance.
 * @id: an application ID, e.g. `gimp.desktop`
 *
 * Add a the suggest application ID.
 *
 * Since: 0.6.1
 **/
void
as_suggest_add_id (AsSuggest *suggest, const gchar *id)
{
	AsSuggestPrivate *priv = GET_PRIVATE (suggest);
	g_ptr_array_add (priv->ids, as_ref_string_new (id));
}

/**
 * as_suggest_node_insert: (skip)
 * @suggest: a #AsSuggest instance.
 * @parent: the parent #GNode to use..
 * @ctx: the #AsNodeContext
 *
 * Inserts the suggest into the DOM tree.
 *
 * Returns: (transfer none): A populated #GNode
 *
 * Since: 0.6.1
 **/
GNode *
as_suggest_node_insert (AsSuggest *suggest, GNode *parent, AsNodeContext *ctx)
{
	AsSuggestPrivate *priv = GET_PRIVATE (suggest);
	GNode *n;
	guint i;

	n = as_node_insert (parent, "suggests", NULL,
			    AS_NODE_INSERT_FLAG_NONE,
			    NULL);
	if (priv->kind != AS_SUGGEST_KIND_UNKNOWN) {
		as_node_add_attribute (n,
				       "type",
				       as_suggest_kind_to_string (priv->kind));
	}
	for (i = 0; i < priv->ids->len; i++) {
		const gchar *id = g_ptr_array_index (priv->ids, i);
		as_node_insert (n, "id", id,
				AS_NODE_INSERT_FLAG_NONE,
				NULL);
	}
	return n;
}

/**
 * as_suggest_node_parse:
 * @suggest: a #AsSuggest instance.
 * @node: a #GNode.
 * @ctx: a #AsNodeContext.
 * @error: A #GError or %NULL.
 *
 * Populates the object from a DOM node.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.6.1
 **/
gboolean
as_suggest_node_parse (AsSuggest *suggest, GNode *node,
		       AsNodeContext *ctx, GError **error)
{
	AsNode *c;
	const gchar *tmp;

	tmp = as_node_get_attribute (node, "type");
	if (tmp != NULL)
		as_suggest_set_kind (suggest, as_suggest_kind_from_string (tmp));
	for (c = node->children; c != NULL; c = c->next) {
		if (as_node_get_tag (c) == AS_TAG_ID)
			as_suggest_add_id (suggest, as_node_get_data (c));
	}
	return TRUE;
}

/**
 * as_suggest_node_parse_dep11:
 * @suggest: a #AsSuggest instance.
 * @node: a #GNode.
 * @ctx: a #AsNodeContext.
 * @error: A #GError or %NULL.
 *
 * Populates the object from a DEP-11 node.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.6.1
 **/
gboolean
as_suggest_node_parse_dep11 (AsSuggest *im, GNode *node,
			     AsNodeContext *ctx, GError **error)
{
	return TRUE;
}

/**
 * as_suggest_new:
 *
 * Creates a new #AsSuggest.
 *
 * Returns: (transfer full): a #AsSuggest
 *
 * Since: 0.6.1
 **/
AsSuggest *
as_suggest_new (void)
{
	AsSuggest *suggest;
	suggest = g_object_new (AS_TYPE_SUGGEST, NULL);
	return AS_SUGGEST (suggest);
}

