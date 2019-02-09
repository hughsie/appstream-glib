/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#ifndef ASB_CONTEXT_INTERNAL_H
#define ASB_CONTEXT_INTERNAL_H

#include "asb-context.h"
#include "asb-plugin-loader.h"

GPtrArray	*asb_context_get_file_globs	(AsbContext	*ctx);
GPtrArray	*asb_context_get_packages	(AsbContext	*ctx);
AsbPluginLoader	*asb_context_get_plugin_loader	(AsbContext	*ctx);

G_END_DECLS

#endif /* GPLASB_CONTEXT_INTERNAL_H */
