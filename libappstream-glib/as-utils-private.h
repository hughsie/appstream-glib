/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2016 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#if !defined (__APPSTREAM_GLIB_PRIVATE_H) && !defined (AS_COMPILATION)
#error "Only <appstream-glib.h> can be included directly."
#endif

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "as-utils.h"

G_BEGIN_DECLS

#define		AS_UTILS_UNIQUE_ID_PARTS	6

G_GNUC_INTERNAL
const gchar	*as_hash_lookup_by_locale	(GHashTable	*hash,
						 const gchar	*locale);
G_GNUC_INTERNAL
void		 as_pixbuf_sharpen		(GdkPixbuf	*src,
						 gint		 radius,
						 gdouble	 amount);
G_GNUC_INTERNAL
void		 as_pixbuf_blur			(GdkPixbuf	*src,
						 gint		 radius,
						 gint		 iterations);
G_GNUC_INTERNAL
const gchar	*as_ptr_array_find_string	(GPtrArray	*array,
						 const gchar	*value);
G_GNUC_INTERNAL
gboolean	 as_utils_locale_is_compatible	(const gchar	*locale1,
						 const gchar	*locale2);
G_GNUC_INTERNAL
GDateTime	*as_utils_iso8601_to_datetime	(const gchar	*iso_date);

G_END_DECLS
