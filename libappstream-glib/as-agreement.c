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
 * SECTION:as-agreement
 * @short_description: Object representing a privacy policy
 * @include: appstream-glib.h
 * @stability: Unstable
 *
 * Agreements can be used by components to specify GDPR, EULA or other warnings.
 *
 * See also: #AsAgreementDetail, #AsAgreementSection
 */

#include "config.h"

#include "as-node-private.h"
#include "as-agreement-private.h"
#include "as-agreement-section-private.h"
#include "as-ref-string.h"
#include "as-tag.h"

typedef struct {
	AsAgreementKind		 kind;
	AsRefString		*version_id;
	GPtrArray		*sections;
} AsAgreementPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsAgreement, as_agreement, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_agreement_get_instance_private (o))

static void
as_agreement_finalize (GObject *object)
{
	AsAgreement *agreement = AS_AGREEMENT (object);
	AsAgreementPrivate *priv = GET_PRIVATE (agreement);

	if (priv->version_id != NULL)
		as_ref_string_unref (priv->version_id);
	g_ptr_array_unref (priv->sections);

	G_OBJECT_CLASS (as_agreement_parent_class)->finalize (object);
}

static void
as_agreement_init (AsAgreement *agreement)
{
	AsAgreementPrivate *priv = GET_PRIVATE (agreement);
	priv->sections = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
}

static void
as_agreement_class_init (AsAgreementClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_agreement_finalize;
}

/**
 * as_agreement_kind_to_string:
 * @value: the #AsAgreementKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @value
 *
 * Since: 0.7.8
 **/
const gchar *
as_agreement_kind_to_string (AsAgreementKind value)
{
	if (value == AS_AGREEMENT_KIND_GENERIC)
		return "generic";
	if (value == AS_AGREEMENT_KIND_EULA)
		return "eula";
	if (value == AS_AGREEMENT_KIND_PRIVACY)
		return "privacy";
	return "unknown";
}

/**
 * as_agreement_kind_from_string:
 * @value: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AsAgreementKind or %AS_AGREEMENT_KIND_UNKNOWN for unknown
 *
 * Since: 0.7.8
 **/
AsAgreementKind
as_agreement_kind_from_string (const gchar *value)
{
	if (g_strcmp0 (value, "generic") == 0)
		return AS_AGREEMENT_KIND_GENERIC;
	if (g_strcmp0 (value, "eula") == 0)
		return AS_AGREEMENT_KIND_EULA;
	if (g_strcmp0 (value, "privacy") == 0)
		return AS_AGREEMENT_KIND_PRIVACY;
	return AS_AGREEMENT_KIND_UNKNOWN;
}

/**
 * as_agreement_get_kind:
 * @agreement: a #AsAgreement instance.
 *
 * Gets the agreement kind.
 *
 * Returns: a string, e.g. %AS_AGREEMENT_KIND_EULA
 *
 * Since: 0.7.8
 **/
AsAgreementKind
as_agreement_get_kind (AsAgreement *agreement)
{
	AsAgreementPrivate *priv = GET_PRIVATE (agreement);
	return priv->kind;
}

/**
 * as_agreement_set_kind:
 * @agreement: a #AsAgreement instance.
 * @kind: the agreement kind, e.g. %AS_AGREEMENT_KIND_EULA
 *
 * Sets the agreement kind.
 *
 * Since: 0.7.8
 **/
void
as_agreement_set_kind (AsAgreement *agreement, AsAgreementKind kind)
{
	AsAgreementPrivate *priv = GET_PRIVATE (agreement);
	priv->kind = kind;
}

/**
 * as_agreement_get_version_id:
 * @agreement: a #AsAgreement instance.
 *
 * Gets the agreement version_id.
 *
 * Returns: a string, e.g. "1.4a", or NULL
 *
 * Since: 0.7.8
 **/
const gchar *
as_agreement_get_version_id (AsAgreement *agreement)
{
	AsAgreementPrivate *priv = GET_PRIVATE (agreement);
	return priv->version_id;
}

/**
 * as_agreement_set_version_id:
 * @agreement: a #AsAgreement instance.
 * @version_id: the agreement version ID, e.g. "1.4a"
 *
 * Sets the agreement version identifier.
 *
 * Since: 0.7.8
 **/
void
as_agreement_set_version_id (AsAgreement *agreement, const gchar *version_id)
{
	AsAgreementPrivate *priv = GET_PRIVATE (agreement);
	as_ref_string_assign_safe (&priv->version_id, version_id);
}

/**
 * as_agreement_get_sections:
 * @agreement: a #AsAgreement instance.
 *
 * Gets all the sections in the agreement.
 *
 * Returns: (transfer container) (element-type AsAgreementSection): array
 *
 * Since: 0.7.8
 **/
GPtrArray *
as_agreement_get_sections (AsAgreement *agreement)
{
	AsAgreementPrivate *priv = GET_PRIVATE (agreement);
	return priv->sections;
}

/**
 * as_agreement_get_section_default:
 * @agreement: a #AsAgreement instance.
 *
 * Gets the first section in the agreement.
 *
 * Returns: (transfer none): agreement section, or %NULL
 *
 * Since: 0.7.8
 **/
AsAgreementSection *
as_agreement_get_section_default (AsAgreement *agreement)
{
	AsAgreementPrivate *priv = GET_PRIVATE (agreement);
	if (priv->sections->len == 0)
		return NULL;
	return AS_AGREEMENT_SECTION (g_ptr_array_index (priv->sections, 0));
}

/**
 * as_agreement_add_detail:
 * @agreement: a #AsAgreement instance.
 * @agreement_section: a #AsAgreementSection instance.
 *
 * Adds a section to the agreement.
 *
 * Since: 0.7.8
 **/
void
as_agreement_add_section (AsAgreement *agreement, AsAgreementSection *agreement_section)
{
	AsAgreementPrivate *priv = GET_PRIVATE (agreement);
	g_ptr_array_add (priv->sections, g_object_ref (agreement_section));
}

/**
 * as_agreement_node_insert: (skip)
 * @agreement: a #AsAgreement instance.
 * @parent: the parent #GNode to use..
 * @ctx: the #AsNodeContext
 *
 * Inserts the agreement into the DOM tree.
 *
 * Returns: (transfer none): A populated #GNode, or %NULL
 *
 * Since: 0.7.8
 **/
GNode *
as_agreement_node_insert (AsAgreement *agreement,
			       GNode *parent,
			       AsNodeContext *ctx)
{
	AsAgreementPrivate *priv = GET_PRIVATE (agreement);
	GNode *n = as_node_insert (parent, "agreement", NULL,
				   AS_NODE_INSERT_FLAG_NONE,
				   NULL);
	if (priv->kind != AS_AGREEMENT_KIND_UNKNOWN) {
		as_node_add_attribute (n, "type",
				       as_agreement_kind_to_string (priv->kind));
	}
	if (priv->version_id != NULL)
		as_node_add_attribute (n, "version_id", priv->version_id);
	for (guint i = 0; i < priv->sections->len; i++) {
		AsAgreementSection *ps = g_ptr_array_index (priv->sections, i);
		as_agreement_section_node_insert (ps, n, ctx);
	}

	return n;
}

/**
 * as_agreement_node_parse:
 * @agreement: a #AsAgreement instance.
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
as_agreement_node_parse (AsAgreement *agreement, GNode *node,
			      AsNodeContext *ctx, GError **error)
{
	const gchar *tmp;

	/* get ID */
	tmp = as_node_get_attribute (node, "type");
	if (tmp != NULL)
		as_agreement_set_kind (agreement, as_agreement_kind_from_string (tmp));
	tmp = as_node_get_attribute (node, "version_id");
	if (tmp != NULL)
		as_agreement_set_version_id (agreement, tmp);

	/* get sections and details */
	for (GNode *c = node->children; c != NULL; c = c->next) {
		if (as_node_get_tag (c) == AS_TAG_AGREEMENT_SECTION) {
			g_autoptr(AsAgreementSection) ps = as_agreement_section_new ();
			if (!as_agreement_section_node_parse (ps, c, ctx, error))
				return FALSE;
			as_agreement_add_section (agreement, ps);
			continue;
		}
	}
	return TRUE;
}

/**
 * as_agreement_new:
 *
 * Creates a new #AsAgreement.
 *
 * Returns: (transfer full): a #AsAgreement
 *
 * Since: 0.7.8
 **/
AsAgreement *
as_agreement_new (void)
{
	AsAgreement *agreement;
	agreement = g_object_new (AS_TYPE_AGREEMENT, NULL);
	return AS_AGREEMENT (agreement);
}
