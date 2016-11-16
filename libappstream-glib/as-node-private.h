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

#if !defined (__APPSTREAM_GLIB_PRIVATE_H) && !defined (AS_COMPILATION)
#error "Only <appstream-glib.h> can be included directly."
#endif

#ifndef __AS_NODE_PRIVATE_H
#define __AS_NODE_PRIVATE_H

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
AsAppSourceKind	 as_node_context_get_source_kind (AsNodeContext	*ctx);
void		 as_node_context_set_source_kind (AsNodeContext	*ctx,
						 AsAppSourceKind source_kind);
gboolean	 as_node_context_get_output_trusted (AsNodeContext	*ctx);
void		 as_node_context_set_output_trusted (AsNodeContext	*ctx,
						 gboolean output_trusted);
AsAppSourceKind	 as_node_context_get_output	(AsNodeContext	*ctx);
void		 as_node_context_set_output	(AsNodeContext	*ctx,
						 AsAppSourceKind output);
const gchar	*as_node_context_get_media_base_url (AsNodeContext	*ctx);
void		 as_node_context_set_media_base_url (AsNodeContext	*ctx,
						     const gchar	*url);

AsRefString	*as_node_reflow_text		(const gchar	*text,
						 gssize		 text_len);
gchar		*as_node_fix_locale		(const gchar	*locale);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(AsNodeContext, as_node_context_free)

G_END_DECLS

#endif /* __AS_NODE_PRIVATE_H */

