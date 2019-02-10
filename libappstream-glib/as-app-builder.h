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

#include <glib.h>
#include <gio/gio.h>

#include "as-app.h"

G_BEGIN_DECLS

/**
 * AsAppBuilderFlags:
 * @AS_APP_BUILDER_FLAG_NONE:			No special actions to use
 * @AS_APP_BUILDER_FLAG_USE_FALLBACKS:		Fall back to guesses where required
 *
 * The flags to use when building applications.
 **/
typedef enum {
	AS_APP_BUILDER_FLAG_NONE,
	AS_APP_BUILDER_FLAG_USE_FALLBACKS	= 1,	/* Since: 0.5.8 */
	/*< private >*/
	AS_APP_BUILDER_FLAG_LAST,
} AsAppBuilderFlags;

gboolean	 as_app_builder_search_translations	(AsApp		*app,
							 const gchar	*prefix,
							 guint		 min_percentage,
							 AsAppBuilderFlags flags,
							 GCancellable	*cancellable,
							 GError		**error);
gboolean	 as_app_builder_search_kudos		(AsApp		*app,
							 const gchar	*prefix,
							 AsAppBuilderFlags flags,
							 GError		**error);
gboolean	 as_app_builder_search_provides		(AsApp		*app,
							 const gchar	*prefix,
							 AsAppBuilderFlags flags,
							 GError		**error);

G_END_DECLS
