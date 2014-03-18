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

#ifndef __AS_ENUMS_H
#define __AS_ENUMS_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
	AS_ID_KIND_UNKNOWN,
	AS_ID_KIND_DESKTOP,
	AS_ID_KIND_FONT,
	AS_ID_KIND_CODEC,
	AS_ID_KIND_INPUT_METHOD,
	AS_ID_KIND_WEB_APP,
	AS_ID_KIND_SOURCE,
	/*< private >*/
	AS_ID_KIND_LAST
} AsIdKind;

typedef enum {
	AS_ICON_KIND_UNKNOWN,
	AS_ICON_KIND_STOCK,
	AS_ICON_KIND_CACHED,
	AS_ICON_KIND_REMOTE,
	/*< private >*/
	AS_ICON_KIND_LAST
} AsIconKind;

typedef enum {
	AS_URL_KIND_UNKNOWN,
	AS_URL_KIND_HOMEPAGE,
	/*< private >*/
	AS_URL_KIND_LAST
} AsUrlKind;

const gchar	*as_id_kind_to_string		(AsIdKind	 id_kind);
AsIdKind	 as_id_kind_from_string		(const gchar	*id_kind);

const gchar	*as_icon_kind_to_string		(AsIconKind	 icon_kind);
AsIconKind	 as_icon_kind_from_string	(const gchar	*icon_kind);

const gchar	*as_url_kind_to_string		(AsUrlKind	 url_kind);
AsUrlKind	 as_url_kind_from_string	(const gchar	*url_kind);

G_END_DECLS

#endif /* __AS_ENUMS_H */
