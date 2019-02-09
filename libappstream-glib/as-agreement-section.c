/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

/**
 * SECTION:as-agreement-section
 * @short_description: Object representing a agreement section
 * @include: appstream-glib.h
 * @stability: Unstable
 *
 * Agreements are typically split up into sections.
 *
 * See also: #AsAgreement, #AsAgreementDetail
 */

#include "config.h"

#include "as-node-private.h"
#include "as-agreement-section-private.h"
#include "as-ref-string.h"
#include "as-tag.h"
#include "as-utils-private.h"

typedef struct {
	AsRefString		*kind;
	GHashTable		*names;		/* of AsRefString:AsRefString */
	GHashTable		*descriptions;	/* of AsRefString:AsRefString */
} AsAgreementSectionPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsAgreementSection, as_agreement_section, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_agreement_section_get_instance_private (o))

static void
as_agreement_section_finalize (GObject *object)
{
	AsAgreementSection *agreement_section = AS_AGREEMENT_SECTION (object);
	AsAgreementSectionPrivate *priv = GET_PRIVATE (agreement_section);

	if (priv->kind != NULL)
		as_ref_string_unref (priv->kind);
	g_hash_table_unref (priv->names);
	g_hash_table_unref (priv->descriptions);

	G_OBJECT_CLASS (as_agreement_section_parent_class)->finalize (object);
}

static void
as_agreement_section_init (AsAgreementSection *agreement_section)
{
	AsAgreementSectionPrivate *priv = GET_PRIVATE (agreement_section);
	priv->names = g_hash_table_new_full (g_str_hash, g_str_equal,
					     (GDestroyNotify) as_ref_string_unref,
					     (GDestroyNotify) as_ref_string_unref);
	priv->descriptions = g_hash_table_new_full (g_str_hash, g_str_equal,
						    (GDestroyNotify) as_ref_string_unref,
						    (GDestroyNotify) as_ref_string_unref);
}

static void
as_agreement_section_class_init (AsAgreementSectionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_agreement_section_finalize;
}

/**
 * as_agreement_section_get_kind:
 * @agreement_section: a #AsAgreementSection instance.
 *
 * Gets the agreement section kind.
 *
 * Returns: a string, e.g. "GDPR", or NULL
 *
 * Since: 0.7.8
 **/
const gchar *
as_agreement_section_get_kind (AsAgreementSection *agreement_section)
{
	AsAgreementSectionPrivate *priv = GET_PRIVATE (agreement_section);
	g_return_val_if_fail (AS_IS_AGREEMENT_SECTION (agreement_section), NULL);
	return priv->kind;
}

/**
 * as_agreement_section_set_kind:
 * @agreement_section: a #AsAgreementSection instance.
 * @kind: the rating kind, e.g. "GDPR"
 *
 * Sets the agreement section kind.
 *
 * Since: 0.7.8
 **/
void
as_agreement_section_set_kind (AsAgreementSection *agreement_section, const gchar *kind)
{
	AsAgreementSectionPrivate *priv = GET_PRIVATE (agreement_section);
	g_return_if_fail (AS_IS_AGREEMENT_SECTION (agreement_section));
	as_ref_string_assign_safe (&priv->kind, kind);
}

/**
 * as_agreement_section_get_name:
 * @agreement_section: a #AsAgreementSection instance.
 * @locale: (nullable): the locale. e.g. "en_GB"
 *
 * Gets the agreement section name.
 *
 * Returns: a string, e.g. "GDPR", or NULL
 *
 * Since: 0.7.8
 **/
const gchar *
as_agreement_section_get_name (AsAgreementSection *agreement_section, const gchar *locale)
{
	AsAgreementSectionPrivate *priv = GET_PRIVATE (agreement_section);
	g_return_val_if_fail (AS_IS_AGREEMENT_SECTION (agreement_section), NULL);
	return as_hash_lookup_by_locale (priv->names, locale);
}

/**
 * as_agreement_section_set_name:
 * @agreement_section: a #AsAgreementSection instance.
 * @locale: (nullable): the locale. e.g. "en_GB"
 * @name: the rating name, e.g. "GDPR"
 *
 * Sets the agreement section name.
 *
 * Since: 0.7.8
 **/
void
as_agreement_section_set_name (AsAgreementSection *agreement_section,
			       const gchar *locale, const gchar *name)
{
	AsAgreementSectionPrivate *priv = GET_PRIVATE (agreement_section);
	g_autoptr(AsRefString) locale_fixed = NULL;

	g_return_if_fail (AS_IS_AGREEMENT_SECTION (agreement_section));
	g_return_if_fail (name != NULL);

	/* get fixed locale */
	locale_fixed = as_node_fix_locale (locale);
	if (locale_fixed == NULL)
		return;
	g_hash_table_insert (priv->names,
			     as_ref_string_ref (locale_fixed),
			     as_ref_string_new (name));
}

/**
 * as_agreement_section_get_description:
 * @agreement_section: a #AsAgreementSection instance.
 * @locale: (nullable): the locale. e.g. "en_GB"
 *
 * Gets the agreement section desc.
 *
 * Returns: a string, e.g. "GDPR", or NULL
 *
 * Since: 0.7.8
 **/
const gchar *
as_agreement_section_get_description (AsAgreementSection *agreement_section,
				      const gchar *locale)
{
	AsAgreementSectionPrivate *priv = GET_PRIVATE (agreement_section);
	g_return_val_if_fail (AS_IS_AGREEMENT_SECTION (agreement_section), NULL);
	return as_hash_lookup_by_locale (priv->descriptions, locale);
}

/**
 * as_agreement_section_set_description:
 * @agreement_section: a #AsAgreementSection instance.
 * @locale: (nullable): the locale. e.g. "en_GB"
 * @desc: the rating desc, e.g. "GDPR"
 *
 * Sets the agreement section description.
 *
 * Since: 0.7.8
 **/
void
as_agreement_section_set_description (AsAgreementSection *agreement_section,
				      const gchar *locale, const gchar *desc)
{
	AsAgreementSectionPrivate *priv = GET_PRIVATE (agreement_section);
	g_autoptr(AsRefString) locale_fixed = NULL;

	g_return_if_fail (AS_IS_AGREEMENT_SECTION (agreement_section));
	g_return_if_fail (desc != NULL);

	/* get fixed locale */
	locale_fixed = as_node_fix_locale (locale);
	if (locale_fixed == NULL)
		return;
	g_hash_table_insert (priv->descriptions,
			     as_ref_string_ref (locale_fixed),
			     as_ref_string_new (desc));
}

/**
 * as_agreement_section_node_insert: (skip)
 * @agreement_section: a #AsAgreementSection instance.
 * @parent: the parent #GNode to use..
 * @ctx: the #AsNodeContext
 *
 * Inserts the agreement_section into the DOM tree.
 *
 * Returns: (transfer none): A populated #GNode, or %NULL
 *
 * Since: 0.7.8
 **/
GNode *
as_agreement_section_node_insert (AsAgreementSection *agreement_section,
				  GNode *parent,
				  AsNodeContext *ctx)
{
	AsAgreementSectionPrivate *priv = GET_PRIVATE (agreement_section);
	g_return_val_if_fail (AS_IS_AGREEMENT_SECTION (agreement_section), NULL);
	GNode *n = as_node_insert (parent, "agreement_section", NULL,
				   AS_NODE_INSERT_FLAG_NONE,
				   NULL);
	if (priv->kind != NULL)
		as_node_add_attribute (n, "type", priv->kind);
	as_node_insert_localized (n, "name",
				  priv->names,
				  AS_NODE_INSERT_FLAG_DEDUPE_LANG);
	as_node_insert_localized (n, "description",
				  priv->descriptions,
				  AS_NODE_INSERT_FLAG_PRE_ESCAPED |
				  AS_NODE_INSERT_FLAG_DEDUPE_LANG);

	return n;
}

static void
as_agreement_section_copy_dict (GHashTable *dest, GHashTable *src)
{
	g_autoptr(GList) keys = g_hash_table_get_keys (src);
	for (GList *l = keys; l != NULL; l = l->next) {
		AsRefString *key = l->data;
		AsRefString *value = g_hash_table_lookup (src, key);
		g_hash_table_insert (dest, as_ref_string_ref (key), as_ref_string_ref (value));
	}
}

/**
 * as_agreement_section_node_parse:
 * @agreement_section: a #AsAgreementSection instance.
 * @node: a #GNode.
 * @ctx: a #AsNodeContext.
 * @error: A #GError or %NULL.
 *
 * Populates the object from a DOM node.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.7.8
 **/
gboolean
as_agreement_section_node_parse (AsAgreementSection *agreement_section, GNode *n,
				 AsNodeContext *ctx, GError **error)
{
	AsAgreementSectionPrivate *priv = GET_PRIVATE (agreement_section);
	const gchar *tmp;
	AsRefString *str;

	/* get ID */
	tmp = as_node_get_attribute (n, "type");
	if (tmp != NULL)
		as_agreement_section_set_kind (agreement_section, tmp);

	/* get sections and details */
	for (GNode *c = n->children; c != NULL; c = c->next) {
		if (as_node_get_tag (c) == AS_TAG_NAME) {
			g_autoptr(AsRefString) xml_lang = NULL;
			xml_lang = as_node_fix_locale_full (n, as_node_get_attribute (n, "xml:lang"));
			if (xml_lang == NULL)
				break;
			str = as_node_get_data_as_refstr (n);
			if (str != NULL) {
				g_hash_table_insert (priv->names,
						     as_ref_string_ref (xml_lang),
						     as_ref_string_ref (str));
			}
			continue;
		}
		if (as_node_get_tag (c) == AS_TAG_DESCRIPTION) {
			g_autoptr(GHashTable) desc = NULL;
			desc = as_node_get_localized_unwrap (c, error);
			if (desc == NULL)
				return FALSE;
			as_agreement_section_copy_dict (priv->descriptions, desc);
			continue;
		}
	}
	return TRUE;
}

/**
 * as_agreement_section_new:
 *
 * Creates a new #AsAgreementSection.
 *
 * Returns: (transfer full): a #AsAgreementSection
 *
 * Since: 0.7.8
 **/
AsAgreementSection *
as_agreement_section_new (void)
{
	AsAgreementSection *agreement_section;
	agreement_section = g_object_new (AS_TYPE_AGREEMENT_SECTION, NULL);
	return AS_AGREEMENT_SECTION (agreement_section);
}
