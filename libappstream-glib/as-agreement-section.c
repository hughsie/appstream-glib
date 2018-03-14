/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
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

typedef struct {
	AsRefString		*kind;
	AsRefString		*name;
	AsRefString		*desc;
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
	if (priv->name != NULL)
		as_ref_string_unref (priv->name);
	if (priv->desc != NULL)
		as_ref_string_unref (priv->desc);

	G_OBJECT_CLASS (as_agreement_section_parent_class)->finalize (object);
}

static void
as_agreement_section_init (AsAgreementSection *agreement_section)
{
//	AsAgreementSectionPrivate *priv = GET_PRIVATE (agreement_section);
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
	return priv->name;
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
	as_ref_string_assign_safe (&priv->name, name);
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
	return priv->desc;
}

/**
 * as_agreement_section_set_description:
 * @agreement_section: a #AsAgreementSection instance.
 * @locale: (nullable): the locale. e.g. "en_GB"
 * @desc: the rating desc, e.g. "GDPR"
 *
 * Sets the agreement section desc.
 *
 * Since: 0.7.8
 **/
void
as_agreement_section_set_description (AsAgreementSection *agreement_section,
				      const gchar *locale, const gchar *desc)
{
	AsAgreementSectionPrivate *priv = GET_PRIVATE (agreement_section);
	as_ref_string_assign_safe (&priv->desc, desc);
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
	GNode *n = as_node_insert (parent, "agreement_section", NULL,
				   AS_NODE_INSERT_FLAG_NONE,
				   NULL);
	if (priv->kind != NULL)
		as_node_add_attribute (n, "type", priv->kind);
	if (priv->desc != NULL) {
		as_node_insert (n, "description", priv->desc,
				AS_NODE_INSERT_FLAG_PRE_ESCAPED, NULL);
	}

	return n;
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
as_agreement_section_node_parse (AsAgreementSection *agreement_section, GNode *node,
				 AsNodeContext *ctx, GError **error)
{
	const gchar *tmp;

	/* get ID */
	tmp = as_node_get_attribute (node, "type");
	if (tmp != NULL)
		as_agreement_section_set_kind (agreement_section, tmp);

	/* get sections and details */
	for (GNode *c = node->children; c != NULL; c = c->next) {
		if (as_node_get_tag (c) == AS_TAG_NAME) {
			as_agreement_section_set_name (agreement_section,
						       as_node_get_attribute (c, "xml:lang"),
						       as_node_get_data (c));
			continue;
		}
		if (as_node_get_tag (c) == AS_TAG_DESCRIPTION) {
			g_autoptr(GString) xml = NULL;
			xml = as_node_to_xml (c->children,
					      AS_NODE_TO_XML_FLAG_INCLUDE_SIBLINGS);
			as_agreement_section_set_description (agreement_section,
							      as_node_get_attribute (c, "xml:lang"),
							      xml->str);
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
