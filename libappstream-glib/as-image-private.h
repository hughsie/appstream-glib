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

#include "as-image.h"
#include "as-node-private.h"

G_BEGIN_DECLS

GNode		*as_image_node_insert		(AsImage	*image,
						 GNode		*parent,
						 AsNodeContext	*ctx);
gboolean	 as_image_node_parse		(AsImage	*image,
						 GNode		*node,
						 AsNodeContext	*ctx,
						 GError		**error);
gboolean	 as_image_node_parse_dep11	(AsImage	*image,
						 GNode		*node,
						 AsNodeContext	*ctx,
						 GError		**error);
void		 as_image_set_url_rstr		(AsImage	*image,
						 AsRefString	*rstr);

G_END_DECLS
