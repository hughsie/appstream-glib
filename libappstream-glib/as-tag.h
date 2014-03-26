/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com`
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

/**
 * AsTag:
 * @AS_TAG_UNKNOWN:			Type invalid or not known
 * @AS_TAG_APPLICATIONS:		`applications`
 * @AS_TAG_APPLICATION:			`application`
 * @AS_TAG_ID:				`id`
 * @AS_TAG_PKGNAME:			`pkgname`
 * @AS_TAG_NAME:			`name`
 * @AS_TAG_SUMMARY:			`summary`
 * @AS_TAG_DESCRIPTION:			`description`
 * @AS_TAG_URL:				`url`
 * @AS_TAG_ICON:			`icon`
 * @AS_TAG_CATEGORIES:			`categories` (or `appcategories`)
 * @AS_TAG_CATEGORY:			`category` (or `appcategory`)
 * @AS_TAG_KEYWORDS:			`keywords`
 * @AS_TAG_KEYWORD:			`keyword`
 * @AS_TAG_MIMETYPES:			`mimetypes`
 * @AS_TAG_MIMETYPE:			`mimetype`
 * @AS_TAG_PROJECT_GROUP:		`project_group`
 * @AS_TAG_PROJECT_LICENSE:		`project_license` (or `licence`)
 * @AS_TAG_SCREENSHOT:			`screenshot`
 * @AS_TAG_SCREENSHOTS:			`screenshots`
 * @AS_TAG_UPDATE_CONTACT:		`updatecontact`
 * @AS_TAG_IMAGE:			`image`
 * @AS_TAG_COMPULSORY_FOR_DESKTOP:	`compulsory_for_desktop`
 * @AS_TAG_PRIORITY:			`priority`
 * @AS_TAG_CAPTION:			`caption`
 * @AS_TAG_LANGUAGES:			`languages`
 * @AS_TAG_LANG:			`lang`
 * @AS_TAG_METADATA:			`metadata`
 * @AS_TAG_VALUE:			`value`
 * @AS_TAG_RELEASES:			`releases`
 * @AS_TAG_RELEASE:			`release`
 * @AS_TAG_ARCHITECTURES:		`architectures`
 * @AS_TAG_ARCH:			`arch`
 *
 * The tag type.
 **/
typedef enum {
	AS_TAG_UNKNOWN,			/* Since: 0.1.0 */
	AS_TAG_APPLICATIONS,		/* Since: 0.1.0 */
	AS_TAG_APPLICATION,		/* Since: 0.1.0 */
	AS_TAG_ID,			/* Since: 0.1.0 */
	AS_TAG_PKGNAME,			/* Since: 0.1.0 */
	AS_TAG_NAME,			/* Since: 0.1.0 */
	AS_TAG_SUMMARY,			/* Since: 0.1.0 */
	AS_TAG_DESCRIPTION,		/* Since: 0.1.0 */
	AS_TAG_URL,			/* Since: 0.1.0 */
	AS_TAG_ICON,			/* Since: 0.1.0 */
	AS_TAG_CATEGORIES,		/* Since: 0.1.0 */
	AS_TAG_CATEGORY,		/* Since: 0.1.0 */
	AS_TAG_KEYWORDS,		/* Since: 0.1.0 */
	AS_TAG_KEYWORD,			/* Since: 0.1.0 */
	AS_TAG_MIMETYPES,		/* Since: 0.1.0 */
	AS_TAG_MIMETYPE,		/* Since: 0.1.0 */
	AS_TAG_PROJECT_GROUP,		/* Since: 0.1.0 */
	AS_TAG_PROJECT_LICENSE,		/* Since: 0.1.0 */
	AS_TAG_SCREENSHOT,		/* Since: 0.1.0 */
	AS_TAG_SCREENSHOTS,		/* Since: 0.1.0 */
	AS_TAG_UPDATE_CONTACT,		/* Since: 0.1.0 */
	AS_TAG_IMAGE,			/* Since: 0.1.0 */
	AS_TAG_COMPULSORY_FOR_DESKTOP,	/* Since: 0.1.0 */
	AS_TAG_PRIORITY,		/* Since: 0.1.0 */
	AS_TAG_CAPTION,			/* Since: 0.1.0 */
	AS_TAG_LANGUAGES,		/* Since: 0.1.0 */
	AS_TAG_LANG,			/* Since: 0.1.0 */
	AS_TAG_METADATA,		/* Since: 0.1.0 */
	AS_TAG_VALUE,			/* Since: 0.1.0 */
	AS_TAG_RELEASES,		/* Since: 0.1.0 */
	AS_TAG_RELEASE,			/* Since: 0.1.0 */
	AS_TAG_ARCHITECTURES,		/* Since: 0.1.1 */
	AS_TAG_ARCH,			/* Since: 0.1.1 */
	/*< private >*/
	AS_TAG_LAST
} AsTag;

typedef enum {
	AS_TAG_FLAG_NONE,
	AS_TAG_FLAG_USE_FALLBACKS,
	/*< private >*/
	AS_TAG_FLAG_LAST
} AsTagFlags;

AsTag		 as_tag_from_string		(const gchar	*tag);
AsTag		 as_tag_from_string_full	(const gchar	*tag,
						 AsTagFlags	 flags);
const gchar	*as_tag_to_string		(AsTag		 tag);

G_END_DECLS

#endif /* __AS_TAG_H */
