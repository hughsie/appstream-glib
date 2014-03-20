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

/**
 * AsIdKind:
 * @AS_ID_KIND_UNKNOWN:			Type invalid or not known
 * @AS_ID_KIND_DESKTOP:			A desktop application
 * @AS_ID_KIND_FONT:			A font add-on
 * @AS_ID_KIND_CODEC:			A codec add-on
 * @AS_ID_KIND_INPUT_METHOD:		A input method add-on
 * @AS_ID_KIND_WEB_APP:			A web appication
 * @AS_ID_KIND_SOURCE:			A software source
 *
 * The ID type.
 **/
typedef enum {
	AS_ID_KIND_UNKNOWN,		/* Since: 0.1.0 */
	AS_ID_KIND_DESKTOP,		/* Since: 0.1.0 */
	AS_ID_KIND_FONT,		/* Since: 0.1.0 */
	AS_ID_KIND_CODEC,		/* Since: 0.1.0 */
	AS_ID_KIND_INPUT_METHOD,	/* Since: 0.1.0 */
	AS_ID_KIND_WEB_APP,		/* Since: 0.1.0 */
	AS_ID_KIND_SOURCE,		/* Since: 0.1.0 */
	/*< private >*/
	AS_ID_KIND_LAST
} AsIdKind;

/**
 * AsIconKind:
 * @AS_ICON_KIND_UNKNOWN:		Type invalid or not known
 * @AS_ICON_KIND_STOCK:			Stock icon or present in the generic icon theme
 * @AS_ICON_KIND_CACHED:		An icon shipped with the AppStream metadata
 * @AS_ICON_KIND_REMOTE:		An icon referenced by a remote URL
 *
 * The icon type.
 **/
typedef enum {
	AS_ICON_KIND_UNKNOWN,		/* Since: 0.1.0 */
	AS_ICON_KIND_STOCK,		/* Since: 0.1.0 */
	AS_ICON_KIND_CACHED,		/* Since: 0.1.0 */
	AS_ICON_KIND_REMOTE,		/* Since: 0.1.0 */
	/*< private >*/
	AS_ICON_KIND_LAST
} AsIconKind;

/**
 * AsUrlKind:
 * @AS_URL_KIND_UNKNOWN:		Type invalid or not known
 * @AS_URL_KIND_HOMEPAGE:		Application project homepage
 * @AS_URL_KIND_BUGTRACKER:		Application bugtracker
 * @AS_URL_KIND_FAQ:			Application FAQ page
 * @AS_URL_KIND_DONATION:		Application donation page
 *
 * The URL type.
 **/
typedef enum {
	AS_URL_KIND_UNKNOWN,		/* Since: 0.1.0 */
	AS_URL_KIND_HOMEPAGE,		/* Since: 0.1.0 */
	AS_URL_KIND_BUGTRACKER,		/* Since: 0.1.1 */
	AS_URL_KIND_FAQ,		/* Since: 0.1.1 */
	AS_URL_KIND_DONATION,		/* Since: 0.1.1 */
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
