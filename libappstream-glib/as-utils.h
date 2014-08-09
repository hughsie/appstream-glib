/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
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

#if !defined (__APPSTREAM_GLIB_H) && !defined (AS_COMPILATION)
#error "Only <appstream-glib.h> can be included directly."
#endif

#ifndef __AS_UTILS_H
#define __AS_UTILS_H

#include <glib.h>

G_BEGIN_DECLS

gchar		*as_markup_convert_simple	(const gchar	*markup,
						 gssize		 markup_len,
						 GError		**error);
gboolean	 as_utils_is_stock_icon_name	(const gchar	*name);
gboolean	 as_utils_is_spdx_license_id	(const gchar	*license_id);
gboolean	 as_utils_is_environment_id	(const gchar	*environment_id);
gboolean	 as_utils_is_category_id	(const gchar	*category_id);
gboolean	 as_utils_is_blacklisted_id	(const gchar	*desktop_id);
gchar		**as_utils_spdx_license_tokenize (const gchar	*license);
gboolean	 as_utils_check_url_exists	(const gchar	*url,
						 guint		 timeout,
						 GError		**error);
gchar		*as_utils_find_icon_filename	(const gchar	*destdir,
						 const gchar	*search,
						 GError		**error);

G_END_DECLS

#endif /* __AS_UTILS_H */
