/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#if !defined (__APPSTREAM_GLIB_PRIVATE_H) && !defined (AS_COMPILATION)
#error "Only <appstream-glib.h> can be included directly."
#endif

#ifndef __AS_AGREEMENT_PRIVATE_H
#define __AS_AGREEMENT_PRIVATE_H

#include <glib-object.h>

#include "as-agreement.h"
#include "as-node-private.h"

G_BEGIN_DECLS

GNode		*as_agreement_node_insert	(AsAgreement	*agreement,
						 GNode		*parent,
						 AsNodeContext	*ctx);
gboolean	 as_agreement_node_parse	(AsAgreement	*agreement,
						 GNode		*node,
						 AsNodeContext	*ctx,
						 GError		**error);

G_END_DECLS

#endif /* __AS_AGREEMENT_PRIVATE_H */
