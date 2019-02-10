/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2017 Richard Hughes <richard@hughsie.com`
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#if !defined (__APPSTREAM_GLIB_H_INSIDE__) && !defined (AS_COMPILATION)
#error "Only <appstream-glib.h> can be included directly."
#endif

#include <glib.h>

G_BEGIN_DECLS

/**
 * AsTag:
 * @AS_TAG_UNKNOWN:			Type invalid or not known
 * @AS_TAG_COMPONENTS:			`components`
 * @AS_TAG_COMPONENT:			`component`
 * @AS_TAG_ID:				`id`
 * @AS_TAG_PKGNAME:			`pkgname`
 * @AS_TAG_NAME:			`name`
 * @AS_TAG_SUMMARY:			`summary`
 * @AS_TAG_DESCRIPTION:			`description`
 * @AS_TAG_URL:				`url`
 * @AS_TAG_ICON:			`icon`
 * @AS_TAG_CATEGORIES:			`categories`
 * @AS_TAG_CATEGORY:			`category`
 * @AS_TAG_KEYWORDS:			`keywords`
 * @AS_TAG_KEYWORD:			`keyword`
 * @AS_TAG_MIMETYPES:			`mimetypes`
 * @AS_TAG_MIMETYPE:			`mimetype`
 * @AS_TAG_PROJECT_GROUP:		`project_group`
 * @AS_TAG_PROJECT_LICENSE:		`project_license`
 * @AS_TAG_SCREENSHOT:			`screenshot`
 * @AS_TAG_SCREENSHOTS:			`screenshots`
 * @AS_TAG_UPDATE_CONTACT:		`update_contact`
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
 * @AS_TAG_METADATA_LICENSE:		`metadata_license`
 * @AS_TAG_PROVIDES:			`provides`
 * @AS_TAG_EXTENDS:			`extends`
 * @AS_TAG_DEVELOPER_NAME:		`developer_name`
 * @AS_TAG_KUDOS:			`kudos`
 * @AS_TAG_KUDO:			`kudo`
 * @AS_TAG_SOURCE_PKGNAME:		`source_pkgname`
 * @AS_TAG_VETOS:			`vetos`
 * @AS_TAG_VETO:			`veto`
 * @AS_TAG_BUNDLE:			`bundle`
 * @AS_TAG_PERMISSIONS:			`permissions`
 * @AS_TAG_PERMISSION:			`permission`
 * @AS_TAG_LOCATION:			`location`
 * @AS_TAG_CHECKSUM:			`checksum`
 * @AS_TAG_SIZE:			`size`
 * @AS_TAG_TRANSLATION:			`translation`
 * @AS_TAG_CONTENT_RATING:		`content_rating`
 * @AS_TAG_CONTENT_ATTRIBUTE:		`content_attribute`
 * @AS_TAG_VERSION:			`version`
 * @AS_TAG_REVIEWS:			`reviews`
 * @AS_TAG_REVIEW:			`review`
 * @AS_TAG_REVIEWER_NAME:		`reviewer_name`
 * @AS_TAG_REVIEWER_ID:			`reviewer_id`
 * @AS_TAG_SUGGESTS:			`suggests`
 * @AS_TAG_REQUIRES:			`requires`
 * @AS_TAG_CUSTOM:			`custom`
 * @AS_TAG_LAUNCHABLE:			`launchable`
 * @AS_TAG_AGREEMENT:			`agreement`
 * @AS_TAG_AGREEMENT_SECTION:		`agreement_section`
 *
 * The tag type.
 **/
typedef enum {
	AS_TAG_UNKNOWN,			/* Since: 0.1.0 */
	AS_TAG_COMPONENTS,		/* Since: 0.5.0 */
	AS_TAG_COMPONENT,		/* Since: 0.5.0 */
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
	AS_TAG_METADATA_LICENSE,	/* Since: 0.1.4 */
	AS_TAG_PROVIDES,		/* Since: 0.1.6 */
	AS_TAG_EXTENDS,			/* Since: 0.1.7 */
	AS_TAG_DEVELOPER_NAME,		/* Since: 0.1.8 */
	AS_TAG_KUDOS,			/* Since: 0.2.1 */
	AS_TAG_KUDO,			/* Since: 0.2.1 */
	AS_TAG_SOURCE_PKGNAME,		/* Since: 0.2.4 */
	AS_TAG_VETOS,			/* Since: 0.3.0 */
	AS_TAG_VETO,			/* Since: 0.3.0 */
	AS_TAG_BUNDLE,			/* Since: 0.3.5 */
	AS_TAG_PERMISSIONS,		/* Since: 0.3.5 */
	AS_TAG_PERMISSION,		/* Since: 0.3.5 */
	AS_TAG_LOCATION,		/* Since: 0.3.5 */
	AS_TAG_CHECKSUM,		/* Since: 0.3.5 */
	AS_TAG_SIZE,			/* Since: 0.5.2 */
	AS_TAG_TRANSLATION,		/* Since: 0.5.8 */
	AS_TAG_CONTENT_RATING,		/* Since: 0.5.12 */
	AS_TAG_CONTENT_ATTRIBUTE,	/* Since: 0.5.12 */
	AS_TAG_VERSION,			/* Since: 0.6.1 */
	AS_TAG_REVIEWS,			/* Since: 0.6.1 */
	AS_TAG_REVIEW,			/* Since: 0.6.1 */
	AS_TAG_REVIEWER_NAME,		/* Since: 0.6.1 */
	AS_TAG_REVIEWER_ID,		/* Since: 0.6.1 */
	AS_TAG_SUGGESTS,		/* Since: 0.6.1 */
	AS_TAG_REQUIRES,		/* Since: 0.6.7 */
	AS_TAG_CUSTOM,			/* Since: 0.6.8 */
	AS_TAG_LAUNCHABLE,		/* Since: 0.6.13 */
	AS_TAG_AGREEMENT,		/* Since: 0.7.8 */
	AS_TAG_AGREEMENT_SECTION,	/* Since: 0.7.8 */
	AS_TAG_P,			/* Since: 0.7.9 */
	AS_TAG_LI,			/* Since: 0.7.9 */
	AS_TAG_UL,			/* Since: 0.7.9 */
	AS_TAG_OL,			/* Since: 0.7.9 */
	AS_TAG_BINARY,			/* Since: 0.7.9 */
	AS_TAG_FONT,			/* Since: 0.7.9 */
	AS_TAG_DBUS,			/* Since: 0.7.9 */
	AS_TAG_MODALIAS,		/* Since: 0.7.9 */
	AS_TAG_LIBRARY,			/* Since: 0.7.9 */
	/*< private >*/
	AS_TAG_LAST
} AsTag;

/**
 * AsTagFlags:
 * @AS_TAG_FLAG_NONE:			No special actions to use
 * @AS_TAG_FLAG_USE_FALLBACKS:		Use fallback tag names
 * @AS_TAG_FLAG_USE_TRANSLATED:		Use translated tag names
 *
 * The flags to use when matching %AsTag's.
 **/
typedef enum {
	AS_TAG_FLAG_NONE,
	AS_TAG_FLAG_USE_FALLBACKS 	= 1,	/* Since: 0.1.4 */
	AS_TAG_FLAG_USE_TRANSLATED	= 2,	/* Since: 0.1.6 */
	/*< private >*/
	AS_TAG_FLAG_LAST
} AsTagFlags;

AsTag		 as_tag_from_string		(const gchar	*tag);
AsTag		 as_tag_from_string_full	(const gchar	*tag,
						 AsTagFlags	 flags);
const gchar	*as_tag_to_string		(AsTag		 tag);

G_END_DECLS
