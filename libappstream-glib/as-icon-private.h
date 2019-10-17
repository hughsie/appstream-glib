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

#include "as-icon.h"
#include "as-node-private.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL
GBytes		*as_icon_get_data		(AsIcon		*icon);
G_GNUC_INTERNAL
void		 as_icon_set_data		(AsIcon		*icon,
						 GBytes		*data);

G_GNUC_INTERNAL
GNode		*as_icon_node_insert		(AsIcon		*icon,
						 GNode		*parent,
						 AsNodeContext	*ctx);
G_GNUC_INTERNAL
gboolean	 as_icon_node_parse		(AsIcon		*icon,
						 GNode		*node,
						 AsNodeContext	*ctx,
						 GError		**error);
G_GNUC_INTERNAL
gboolean	 as_icon_node_parse_dep11	(AsIcon		*icon,
						 GNode		*node,
						 AsNodeContext	*ctx,
						 GError		**error);

G_GNUC_INTERNAL
void		 as_icon_set_prefix_rstr	(AsIcon		*icon,
						 AsRefString	*rstr);

G_END_DECLS
