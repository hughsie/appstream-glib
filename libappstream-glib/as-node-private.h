/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#if !defined (__APPSTREAM_GLIB_PRIVATE_H) && !defined (AS_COMPILATION)
#error "Only <appstream-glib.h> can be included directly."
#endif

#include "as-app.h"
#include "as-node.h"
#include "as-ref-string.h"

G_BEGIN_DECLS

typedef struct _AsNodeContext	AsNodeContext;
AsNodeContext	*as_node_context_new		(void);
void		 as_node_context_free		(AsNodeContext	*ctx);
gdouble		 as_node_context_get_version	(AsNodeContext	*ctx);
void		 as_node_context_set_version	(AsNodeContext	*ctx,
						 gdouble	 version);
AsFormatKind	 as_node_context_get_format_kind (AsNodeContext	*ctx);
void		 as_node_context_set_format_kind (AsNodeContext	*ctx,
						 AsFormatKind	 format_kind);
/* Kept for ABI compatibility with as-glib < 0.9.6 */
G_DEPRECATED_FOR(as_node_context_get_format_kind)
AsFormatKind	 as_node_context_get_source_kind (AsNodeContext	*ctx);
/* Kept for ABI compatibility with as-glib < 0.9.6 */
G_DEPRECATED_FOR(as_node_context_set_format_kind)
void		 as_node_context_set_source_kind (AsNodeContext	*ctx,
						 AsFormatKind	 source_kind);
gboolean	 as_node_context_get_output_trusted (AsNodeContext	*ctx);
void		 as_node_context_set_output_trusted (AsNodeContext	*ctx,
						 gboolean output_trusted);
AsFormatKind	 as_node_context_get_output	(AsNodeContext	*ctx);
void		 as_node_context_set_output	(AsNodeContext	*ctx,
						 AsFormatKind output);
const gchar	*as_node_context_get_media_base_url (AsNodeContext	*ctx);
void		 as_node_context_set_media_base_url (AsNodeContext	*ctx,
						     const gchar	*url);

AsRefString	*as_node_reflow_text		(const gchar	*text,
						 gssize		 text_len);
AsRefString	*as_node_fix_locale		(const gchar	*locale);
AsRefString	*as_node_fix_locale_full	(const GNode	*node,
						 const gchar	*locale);

AsRefString	*as_node_get_data_as_refstr	(const AsNode	*node);
AsRefString	*as_node_get_attribute_as_refstr (const AsNode	*node,
						const gchar	*key);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(AsNodeContext, as_node_context_free)

G_END_DECLS
