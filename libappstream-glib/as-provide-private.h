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

#include "as-provide.h"
#include "as-node-private.h"

G_BEGIN_DECLS

GNode		*as_provide_node_insert		(AsProvide	*provide,
						 GNode		*parent,
						 AsNodeContext	*ctx);
gboolean	 as_provide_node_parse		(AsProvide	*provide,
						 GNode		*node,
						 AsNodeContext	*ctx,
						 GError		**error);
gboolean	 as_provide_node_parse_dep11	(AsProvide	*provide,
						 GNode		*node,
						 AsNodeContext	*ctx,
						 GError		**error);

G_END_DECLS
