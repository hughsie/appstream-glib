/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2016 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#if !defined (__APPSTREAM_GLIB_PRIVATE_H) && !defined (AS_COMPILATION)
#error "Only <appstream-glib.h> can be included directly."
#endif

#ifndef __AS_UTILS_PRIVATE_H
#define __AS_UTILS_PRIVATE_H

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "as-utils.h"

G_BEGIN_DECLS

const gchar	*as_hash_lookup_by_locale	(GHashTable	*hash,
						 const gchar	*locale);
void		 as_pixbuf_sharpen		(GdkPixbuf	*src,
						 gint		 radius,
						 gdouble	 amount);
void		 as_pixbuf_blur			(GdkPixbuf	*src,
						 gint		 radius,
						 gint		 iterations);
const gchar	*as_ptr_array_find_string	(GPtrArray	*array,
						 const gchar	*value);
gboolean	 as_utils_locale_is_compatible	(const gchar	*locale1,
						 const gchar	*locale2);
GDateTime	*as_utils_iso8601_to_datetime	(const gchar	*iso_date);

G_END_DECLS

#endif /* __AS_UTILS_PRIVATE_H */
