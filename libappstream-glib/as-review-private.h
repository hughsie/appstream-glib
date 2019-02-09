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

#include "as-review.h"
#include "as-node-private.h"

G_BEGIN_DECLS

GNode		*as_review_node_insert		(AsReview	*review,
						 GNode		*parent,
						 AsNodeContext	*ctx);
gboolean	 as_review_node_parse		(AsReview	*review,
						 GNode		*node,
						 AsNodeContext	*ctx,
						 GError		**error);
gboolean	 as_review_node_parse_dep11	(AsReview	*review,
						 GNode		*node,
						 AsNodeContext	*ctx,
						 GError		**error);

G_END_DECLS
