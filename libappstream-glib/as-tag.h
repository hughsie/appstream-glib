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

#ifndef __AS_TAG_H
#define __AS_TAG_H

#include <glib.h>

typedef enum {
	AS_TAG_UNKNOWN,
	AS_TAG_APPLICATIONS,
	AS_TAG_APPLICATION,
	AS_TAG_ID,
	AS_TAG_PKGNAME,
	AS_TAG_NAME,
	AS_TAG_SUMMARY,
	AS_TAG_DESCRIPTION,
	AS_TAG_URL,
	AS_TAG_ICON,
	AS_TAG_APPCATEGORIES,
	AS_TAG_APPCATEGORY,
	AS_TAG_KEYWORDS,
	AS_TAG_KEYWORD,
	AS_TAG_MIMETYPES,
	AS_TAG_MIMETYPE,
	AS_TAG_PROJECT_GROUP,
	AS_TAG_PROJECT_LICENSE,
	AS_TAG_SCREENSHOT,
	AS_TAG_SCREENSHOTS,
	AS_TAG_UPDATECONTACT,
	AS_TAG_IMAGE,
	AS_TAG_COMPULSORY_FOR_DESKTOP,
	AS_TAG_PRIORITY,
	AS_TAG_CAPTION,
	AS_TAG_LANGUAGES,
	AS_TAG_LANG,
	AS_TAG_METADATA,
	AS_TAG_VALUE,
	AS_TAG_RELEASES,
	AS_TAG_RELEASE,
	/*< private >*/
	AS_TAG_LAST
} AsTag;

AsTag		 as_tag_from_string		(const gchar	*tag);
const gchar	*as_tag_to_string		(AsTag		 tag);

G_END_DECLS

#endif /* __AS_TAG_H */
