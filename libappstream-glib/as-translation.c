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
 * SECTION:as-translation
 * @short_description: Object representing a single translation.
 * @include: appstream-glib.h
 * @stability: Stable
 *
 * Translation systems such as gettext install the translated files in a
 * specific location.
 *
 * This object represents translation data for an application.
 *
 * See also: #AsApp
 */

#include "config.h"

#include "as-translation-private.h"
#include "as-node-private.h"
#include "as-ref-string.h"
#include "as-utils-private.h"
#include "as-yaml.h"

typedef struct
{
	AsTranslationKind	 kind;
	AsRefString		*id;
} AsTranslationPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsTranslation, as_translation, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_translation_get_instance_private (o))

static void
as_translation_finalize (GObject *object)
{
	AsTranslation *translation = AS_TRANSLATION (object);
	AsTranslationPrivate *priv = GET_PRIVATE (translation);

	if (priv->id != NULL)
		as_ref_string_unref (priv->id);

	G_OBJECT_CLASS (as_translation_parent_class)->finalize (object);
}

static void
as_translation_init (AsTranslation *translation)
{
}

static void
as_translation_class_init (AsTranslationClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_translation_finalize;
}


/**
 * as_translation_kind_from_string:
 * @kind: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: (transfer full): a #AsTranslationKind, or %AS_TRANSLATION_KIND_UNKNOWN for unknown.
 *
 * Since: 0.5.8
 **/
AsTranslationKind
as_translation_kind_from_string (const gchar *kind)
{
	if (g_strcmp0 (kind, "gettext") == 0)
		return AS_TRANSLATION_KIND_GETTEXT;
	if (g_strcmp0 (kind, "qt") == 0)
		return AS_TRANSLATION_KIND_QT;
	return AS_TRANSLATION_KIND_UNKNOWN;
}

/**
 * as_translation_kind_to_string:
 * @kind: the #AsTranslationKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @kind
 *
 * Since: 0.5.8
 **/
const gchar *
as_translation_kind_to_string (AsTranslationKind kind)
{
	if (kind == AS_TRANSLATION_KIND_GETTEXT)
		return "gettext";
	if (kind == AS_TRANSLATION_KIND_QT)
		return "qt";
	return NULL;
}

/**
 * as_translation_get_id:
 * @translation: a #AsTranslation instance.
 *
 * Gets the ID for this translation.
 *
 * Returns: ID, e.g. "foobar-1.0.2"
 *
 * Since: 0.5.8
 **/
const gchar *
as_translation_get_id (AsTranslation *translation)
{
	AsTranslationPrivate *priv = GET_PRIVATE (translation);
	return priv->id;
}

/**
 * as_translation_get_kind:
 * @translation: a #AsTranslation instance.
 *
 * Gets the translation kind.
 *
 * Returns: the #AsTranslationKind
 *
 * Since: 0.5.8
 **/
AsTranslationKind
as_translation_get_kind (AsTranslation *translation)
{
	AsTranslationPrivate *priv = GET_PRIVATE (translation);
	return priv->kind;
}

/**
 * as_translation_set_id:
 * @translation: a #AsTranslation instance.
 * @id: the URL.
 *
 * Sets the ID for this translation.
 *
 * Since: 0.5.8
 **/
void
as_translation_set_id (AsTranslation *translation, const gchar *id)
{
	AsTranslationPrivate *priv = GET_PRIVATE (translation);
	as_ref_string_assign_safe (&priv->id, id);
}

/**
 * as_translation_set_kind:
 * @translation: a #AsTranslation instance.
 * @kind: the #AsTranslationKind, e.g. %AS_TRANSLATION_KIND_THUMBNAIL.
 *
 * Sets the translation kind.
 *
 * Since: 0.5.8
 **/
void
as_translation_set_kind (AsTranslation *translation, AsTranslationKind kind)
{
	AsTranslationPrivate *priv = GET_PRIVATE (translation);
	priv->kind = kind;
}

/**
 * as_translation_node_insert: (skip)
 * @translation: a #AsTranslation instance.
 * @parent: the parent #GNode to use..
 * @ctx: the #AsNodeContext
 *
 * Inserts the translation into the DOM tree.
 *
 * Returns: (transfer none): A populated #GNode
 *
 * Since: 0.5.8
 **/
GNode *
as_translation_node_insert (AsTranslation *translation, GNode *parent, AsNodeContext *ctx)
{
	AsTranslationPrivate *priv = GET_PRIVATE (translation);
	GNode *n;

	/* invalid */
	if (priv->kind == AS_TRANSLATION_KIND_UNKNOWN)
		return NULL;

	n = as_node_insert (parent, "translation", priv->id,
			    AS_NODE_INSERT_FLAG_NONE,
			    "type", as_translation_kind_to_string (priv->kind),
			    NULL);
	return n;
}

/**
 * as_translation_node_parse:
 * @translation: a #AsTranslation instance.
 * @node: a #GNode.
 * @ctx: a #AsNodeContext.
 * @error: A #GError or %NULL.
 *
 * Populates the object from a DOM node.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.5.8
 **/
gboolean
as_translation_node_parse (AsTranslation *translation, GNode *node,
			   AsNodeContext *ctx, GError **error)
{
	AsTranslationPrivate *priv = GET_PRIVATE (translation);
	const gchar *tmp;

	tmp = as_node_get_attribute (node, "type");
	as_translation_set_kind (translation, as_translation_kind_from_string (tmp));
	as_ref_string_assign (&priv->id, as_node_get_data (node));
	return TRUE;
}

/**
 * as_translation_node_parse_dep11:
 * @translation: a #AsTranslation instance.
 * @node: a #GNode.
 * @ctx: a #AsNodeContext.
 * @error: A #GError or %NULL.
 *
 * Populates the object from a DEP-11 node.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.5.8
 **/
gboolean
as_translation_node_parse_dep11 (AsTranslation *im, GNode *node,
				 AsNodeContext *ctx, GError **error)
{
	GNode *n;
	const gchar *tmp;

	for (n = node->children; n != NULL; n = n->next) {
		tmp = as_yaml_node_get_key (n);
		if (g_strcmp0 (tmp, "id") == 0)
			as_translation_set_id (im, as_yaml_node_get_value (n));
	}
	return TRUE;
}

/**
 * as_translation_new:
 *
 * Creates a new #AsTranslation.
 *
 * Returns: (transfer full): a #AsTranslation
 *
 * Since: 0.5.8
 **/
AsTranslation *
as_translation_new (void)
{
	AsTranslation *translation;
	translation = g_object_new (AS_TYPE_TRANSLATION, NULL);
	return AS_TRANSLATION (translation);
}
