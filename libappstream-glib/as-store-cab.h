/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#if !defined (__APPSTREAM_GLIB_H_INSIDE__) && !defined (AS_COMPILATION)
#error "Only <appstream-glib.h> can be included directly."
#endif

#include <glib-object.h>
#include <gio/gio.h>

#include "as-store.h"

G_BEGIN_DECLS

gboolean	 as_store_cab_from_file		(AsStore	*store,
						 GFile		*file,
						 GCancellable	*cancellable,
						 GError		**error);
gboolean	 as_store_cab_from_bytes	(AsStore	*store,
						 GBytes		*bytes,
						 GCancellable	*cancellable,
						 GError		**error);

G_END_DECLS
