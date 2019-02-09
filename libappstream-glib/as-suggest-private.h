/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#if !defined (__APPSTREAM_GLIB_PRIVATE_H) && !defined (AS_COMPILATION)
#error "Only <appstream-glib.h> can be included directly."
#endif

#include "as-suggest.h"
#include "as-node-private.h"

G_BEGIN_DECLS

GNode		*as_suggest_node_insert		(AsSuggest	*suggest,
						 GNode		*parent,
						 AsNodeContext	*ctx);
gboolean	 as_suggest_node_parse		(AsSuggest	*suggest,
						 GNode		*node,
						 AsNodeContext	*ctx,
						 GError		**error);
gboolean	 as_suggest_node_parse_dep11	(AsSuggest	*suggest,
						 GNode		*node,
						 AsNodeContext	*ctx,
						 GError		**error);

G_END_DECLS
