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
 * SECTION:as-content-rating
 * @short_description: Object representing a content rating
 * @include: appstream-glib.h
 * @stability: Unstable
 *
 * Content ratings are age-specific guidelines for applications.
 *
 * See also: #AsApp
 */

#include "config.h"

#include "as-node-private.h"
#include "as-content-rating-private.h"
#include "as-ref-string.h"
#include "as-tag.h"

typedef struct {
	AsRefString		*id;
	AsContentRatingValue	 value;
} AsContentRatingKey;

typedef struct
{
	AsRefString		*kind;
	GPtrArray		*keys; /* of AsContentRatingKey */
} AsContentRatingPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsContentRating, as_content_rating, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_content_rating_get_instance_private (o))

static void
as_content_rating_finalize (GObject *object)
{
	AsContentRating *content_rating = AS_CONTENT_RATING (object);
	AsContentRatingPrivate *priv = GET_PRIVATE (content_rating);

	if (priv->kind != NULL)
		as_ref_string_unref (priv->kind);
	g_ptr_array_unref (priv->keys);

	G_OBJECT_CLASS (as_content_rating_parent_class)->finalize (object);
}

static void
as_content_rating_key_free (AsContentRatingKey *key)
{
	if (key->id != NULL)
		as_ref_string_unref (key->id);
	g_slice_free (AsContentRatingKey, key);
}

static void
as_content_rating_init (AsContentRating *content_rating)
{
	AsContentRatingPrivate *priv = GET_PRIVATE (content_rating);
	priv->keys = g_ptr_array_new_with_free_func ((GDestroyNotify) as_content_rating_key_free);
}

static void
as_content_rating_class_init (AsContentRatingClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_content_rating_finalize;
}

static gint
ids_sort_cb (gconstpointer id_ptr_a, gconstpointer id_ptr_b)
{
	const gchar *id_a = *((const gchar **) id_ptr_a);
	const gchar *id_b = *((const gchar **) id_ptr_b);

	return g_strcmp0 (id_a, id_b);
}

/**
 * as_content_rating_get_rating_ids:
 * @content_rating: a #AsContentRating
 *
 * Gets the set of ratings IDs which are present in this @content_rating. An
 * example of a ratings ID is `violence-bloodshed`.
 *
 * The IDs are returned in lexicographical order.
 *
 * Returns: (array zero-terminated=1) (transfer container): %NULL-terminated
 *    array of ratings IDs; each ratings ID is owned by the #AsContentRating and
 *    must not be freed, but the container must be freed with g_free()
 *
 * Since: 0.7.15
 **/
const gchar **
as_content_rating_get_rating_ids (AsContentRating *content_rating)
{
	AsContentRatingPrivate *priv = GET_PRIVATE (content_rating);
	GPtrArray *ids = g_ptr_array_new_with_free_func (NULL);
	guint i;

	g_return_val_if_fail (AS_IS_CONTENT_RATING (content_rating), NULL);

	for (i = 0; i < priv->keys->len; i++) {
		AsContentRatingKey *key = g_ptr_array_index (priv->keys, i);
		g_ptr_array_add (ids, key->id);
	}

	g_ptr_array_sort (ids, ids_sort_cb);
	g_ptr_array_add (ids, NULL);  /* NULL terminator */

	return (const gchar **) g_ptr_array_free (g_steal_pointer (&ids), FALSE);
}

/**
 * as_content_rating_get_value:
 * @content_rating: a #AsContentRating
 * @id: A ratings ID, e.g. `violence-bloodshed`.
 *
 * Gets the set value of a content rating key.
 *
 * Returns: the #AsContentRatingValue, or %AS_CONTENT_RATING_VALUE_UNKNOWN
 *
 * Since: 0.6.4
 **/
AsContentRatingValue
as_content_rating_get_value (AsContentRating *content_rating, const gchar *id)
{
	AsContentRatingPrivate *priv = GET_PRIVATE (content_rating);
	g_return_val_if_fail (AS_IS_CONTENT_RATING (content_rating), AS_CONTENT_RATING_VALUE_UNKNOWN);
	guint i;
	for (i = 0; i < priv->keys->len; i++) {
		AsContentRatingKey *key = g_ptr_array_index (priv->keys, i);
		if (g_strcmp0 (key->id, id) == 0)
			return key->value;
	}
	return AS_CONTENT_RATING_VALUE_UNKNOWN;
}

/**
 * as_content_rating_value_to_string:
 * @value: the #AsContentRatingValue.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @value
 *
 * Since: 0.5.12
 **/
const gchar *
as_content_rating_value_to_string (AsContentRatingValue value)
{
	if (value == AS_CONTENT_RATING_VALUE_NONE)
		return "none";
	if (value == AS_CONTENT_RATING_VALUE_MILD)
		return "mild";
	if (value == AS_CONTENT_RATING_VALUE_MODERATE)
		return "moderate";
	if (value == AS_CONTENT_RATING_VALUE_INTENSE)
		return "intense";
	return "unknown";
}

/**
 * as_content_rating_value_from_string:
 * @value: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AsContentRatingValue or %AS_CONTENT_RATING_VALUE_UNKNOWN for unknown
 *
 * Since: 0.5.12
 **/
AsContentRatingValue
as_content_rating_value_from_string (const gchar *value)
{
	if (g_strcmp0 (value, "none") == 0)
		return AS_CONTENT_RATING_VALUE_NONE;
	if (g_strcmp0 (value, "mild") == 0)
		return AS_CONTENT_RATING_VALUE_MILD;
	if (g_strcmp0 (value, "moderate") == 0)
		return AS_CONTENT_RATING_VALUE_MODERATE;
	if (g_strcmp0 (value, "intense") == 0)
		return AS_CONTENT_RATING_VALUE_INTENSE;
	return AS_CONTENT_RATING_VALUE_UNKNOWN;
}

/* The struct definition below assumes we don’t grow more
 * #AsContentRating values. */
G_STATIC_ASSERT (AS_CONTENT_RATING_VALUE_LAST == AS_CONTENT_RATING_VALUE_INTENSE + 1);

static const struct {
	const gchar	*id;
	guint		 csm_age_none;  /* for %AS_CONTENT_RATING_VALUE_NONE */
	guint		 csm_age_mild;  /* for %AS_CONTENT_RATING_VALUE_MILD */
	guint		 csm_age_moderate;  /* for %AS_CONTENT_RATING_VALUE_MODERATE */
	guint		 csm_age_intense;  /* for %AS_CONTENT_RATING_VALUE_INTENSE */
} oars_to_csm_mappings[] =  {
	/* Each @id must only appear once. The set of @csm_age_* values for a
	 * given @id must be complete and non-decreasing. */
	/* v1.0 */
	{ "violence-cartoon",	0, 3, 4, 6 },
	{ "violence-fantasy",	0, 3, 7, 8 },
	{ "violence-realistic",	0, 4, 9, 14 },
	{ "violence-bloodshed",	0, 9, 11, 18 },
	{ "violence-sexual",	0, 18, 18, 18 },
	{ "drugs-alcohol",	0, 11, 13, 16 },
	{ "drugs-narcotics",	0, 12, 14, 17 },
	{ "drugs-tobacco",	0, 10, 13, 13 },
	{ "sex-nudity",		0, 12, 14, 14 },
	{ "sex-themes",		0, 13, 14, 15 },
	{ "language-profanity",	0, 8, 11, 14 },
	{ "language-humor",	0, 3, 8, 14 },
	{ "language-discrimination", 0, 9, 10, 11 },
	{ "money-advertising",	0, 7, 8, 10 },
	{ "money-gambling",	0, 7, 10, 18 },
	{ "money-purchasing",	0, 12, 14, 15 },
	{ "social-chat",	0, 4, 10, 13 },
	{ "social-audio",	0, 15, 15, 15 },
	{ "social-contacts",	0, 12, 12, 12 },
	{ "social-info",	0, 0, 13, 13 },
	{ "social-location",	0, 13, 13, 13 },
	/* v1.1 additions */
	{ "sex-homosexuality",	0, 10, 13, 18 },
	{ "sex-prostitution",	0, 12, 14, 18 },
	{ "sex-adultery",	0, 8, 10, 18 },
	{ "sex-appearance",	0, 10, 10, 15 },
	{ "violence-worship",	0, 13, 15, 18 },
	{ "violence-desecration", 0, 13, 15, 18 },
	{ "violence-slavery",	0, 13, 15, 18 },
};

/**
 * as_content_rating_attribute_to_csm_age:
 * @id: the subsection ID e.g. `violence-cartoon`
 * @value: the #AsContentRatingValue, e.g. %AS_CONTENT_RATING_VALUE_INTENSE
 *
 * Gets the Common Sense Media approved age for a specific rating level.
 *
 * Returns: The age in years, or 0 for no details.
 *
 * Since: 0.7.15
 **/
guint
as_content_rating_attribute_to_csm_age (const gchar *id, AsContentRatingValue value)
{
	if (value == AS_CONTENT_RATING_VALUE_UNKNOWN ||
	    value == AS_CONTENT_RATING_VALUE_LAST)
		return 0;

	for (gsize i = 0; i < G_N_ELEMENTS (oars_to_csm_mappings); i++) {
		if (g_str_equal (id, oars_to_csm_mappings[i].id)) {
			switch (value) {
			case AS_CONTENT_RATING_VALUE_NONE:
				return oars_to_csm_mappings[i].csm_age_none;
			case AS_CONTENT_RATING_VALUE_MILD:
				return oars_to_csm_mappings[i].csm_age_mild;
			case AS_CONTENT_RATING_VALUE_MODERATE:
				return oars_to_csm_mappings[i].csm_age_moderate;
			case AS_CONTENT_RATING_VALUE_INTENSE:
				return oars_to_csm_mappings[i].csm_age_intense;
			case AS_CONTENT_RATING_VALUE_UNKNOWN:
			case AS_CONTENT_RATING_VALUE_LAST:
			default:
				/* Handled above. */
				g_assert_not_reached ();
				return 0;
			}
		}
	}

	/* @id not found. */
	return 0;
}

/**
 * as_content_rating_get_minimum_age:
 * @content_rating: a #AsContentRating
 *
 * Gets the lowest Common Sense Media approved age for the content_rating block.
 * NOTE: these numbers are based on the data and descriptions available from
 * https://www.commonsensemedia.org/about-us/our-mission/about-our-ratings and
 * you may disagree with them.
 *
 * You're free to disagree with these, and of course you should use your own
 * brain to work our if your child is able to cope with the concepts enumerated
 * here. Some 13 year olds may be fine with the concept of mutilation of body
 * parts; others may get nightmares.
 *
 * Returns: The age in years, 0 for no rating, or G_MAXUINT for no details.
 *
 * Since: 0.5.12
 **/
guint
as_content_rating_get_minimum_age (AsContentRating *content_rating)
{
	AsContentRatingPrivate *priv = GET_PRIVATE (content_rating);
	guint i;
	guint csm_age = 0;

	g_return_val_if_fail (AS_IS_CONTENT_RATING (content_rating), 0);

	/* check kind */
	if (g_strcmp0 (priv->kind, "oars-1.0") != 0 &&
	    g_strcmp0 (priv->kind, "oars-1.1") != 0)
		return G_MAXUINT;

	for (i = 0; i < priv->keys->len; i++) {
		AsContentRatingKey *key;
		guint csm_tmp;
		key = g_ptr_array_index (priv->keys, i);
		csm_tmp = as_content_rating_attribute_to_csm_age (key->id, key->value);
		if (csm_tmp > 0 && csm_tmp > csm_age)
			csm_age = csm_tmp;
	}
	return csm_age;
}

/**
 * as_content_rating_get_kind:
 * @content_rating: a #AsContentRating instance.
 *
 * Gets the content_rating kind.
 *
 * Returns: a string, e.g. "oars-1.0", or NULL
 *
 * Since: 0.5.12
 **/
const gchar *
as_content_rating_get_kind (AsContentRating *content_rating)
{
	AsContentRatingPrivate *priv = GET_PRIVATE (content_rating);
	g_return_val_if_fail (AS_IS_CONTENT_RATING (content_rating), NULL);
	return priv->kind;
}

/**
 * as_content_rating_set_kind:
 * @content_rating: a #AsContentRating instance.
 * @kind: the rating kind, e.g. "oars-1.0"
 *
 * Sets the content rating kind.
 *
 * Since: 0.5.12
 **/
void
as_content_rating_set_kind (AsContentRating *content_rating, const gchar *kind)
{
	AsContentRatingPrivate *priv = GET_PRIVATE (content_rating);
	g_return_if_fail (AS_IS_CONTENT_RATING (content_rating));
	as_ref_string_assign_safe (&priv->kind, kind);
}

/**
 * as_content_rating_node_insert: (skip)
 * @content_rating: a #AsContentRating instance.
 * @parent: the parent #GNode to use.
 * @ctx: the #AsNodeContext
 *
 * Inserts the content_rating into the DOM tree.
 *
 * Returns: (transfer none): A populated #GNode, or %NULL
 *
 * Since: 0.5.12
 **/
GNode *
as_content_rating_node_insert (AsContentRating *content_rating,
			       GNode *parent,
			       AsNodeContext *ctx)
{
	AsContentRatingKey *key;
	AsContentRatingPrivate *priv = GET_PRIVATE (content_rating);
	GNode *n;
	guint i;

	g_return_val_if_fail (AS_IS_CONTENT_RATING (content_rating), NULL);

	n = as_node_insert (parent, "content_rating", NULL,
			    AS_NODE_INSERT_FLAG_NONE,
			    NULL);
	if (priv->kind != NULL)
		as_node_add_attribute (n, "type", priv->kind);
	for (i = 0; i < priv->keys->len; i++) {
		const gchar *tmp;
		key = g_ptr_array_index (priv->keys, i);
		tmp = as_content_rating_value_to_string (key->value);
		as_node_insert (n, "content_attribute", tmp,
				AS_NODE_INSERT_FLAG_NONE,
				"id", key->id,
				NULL);
	}
	return n;
}

/**
 * as_content_rating_add_attribute:
 * @content_rating: a #AsContentRating instance.
 * @id: a content rating ID, e.g. `money-gambling`.
 * @value: a #AsContentRatingValue, e.g. %AS_CONTENT_RATING_VALUE_MODERATE.
 *
 * Adds an attribute value to the content rating.
 *
 * Since: 0.7.14
 **/
void
as_content_rating_add_attribute (AsContentRating *content_rating,
				 const gchar *id,
				 AsContentRatingValue value)
{
	AsContentRatingKey *key = g_slice_new0 (AsContentRatingKey);
	AsContentRatingPrivate *priv = GET_PRIVATE (content_rating);

	g_return_if_fail (AS_IS_CONTENT_RATING (content_rating));
	g_return_if_fail (id != NULL);
	g_return_if_fail (value != AS_CONTENT_RATING_VALUE_UNKNOWN);

	key->id = as_ref_string_new (id);
	key->value = value;
	g_ptr_array_add (priv->keys, key);
}

/**
 * as_content_rating_node_parse:
 * @content_rating: a #AsContentRating instance.
 * @node: a #GNode.
 * @ctx: a #AsNodeContext.
 * @error: A #GError or %NULL.
 *
 * Populates the object from a DOM node.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.5.12
 **/
gboolean
as_content_rating_node_parse (AsContentRating *content_rating, GNode *node,
			      AsNodeContext *ctx, GError **error)
{
	AsContentRatingPrivate *priv = GET_PRIVATE (content_rating);
	GNode *c;
	const gchar *tmp;
	g_autoptr(GHashTable) captions = NULL;

	g_return_val_if_fail (AS_IS_CONTENT_RATING (content_rating), FALSE);

	/* get ID */
	tmp = as_node_get_attribute (node, "type");
	if (tmp != NULL)
		as_content_rating_set_kind (content_rating, tmp);

	/* get keys */
	for (c = node->children; c != NULL; c = c->next) {
		AsContentRatingKey *key;
		if (as_node_get_tag (c) != AS_TAG_CONTENT_ATTRIBUTE)
			continue;
		key = g_slice_new0 (AsContentRatingKey);
		as_ref_string_assign (&key->id, as_node_get_attribute_as_refstr (c, "id"));
		key->value = as_content_rating_value_from_string (as_node_get_data (c));
		g_ptr_array_add (priv->keys, key);
	}
	return TRUE;
}

/**
 * as_content_rating_new:
 *
 * Creates a new #AsContentRating.
 *
 * Returns: (transfer full): a #AsContentRating
 *
 * Since: 0.5.12
 **/
AsContentRating *
as_content_rating_new (void)
{
	AsContentRating *content_rating;
	content_rating = g_object_new (AS_TYPE_CONTENT, NULL);
	return AS_CONTENT_RATING (content_rating);
}
