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

#if !defined (__APPSTREAM_GLIB_PRIVATE_H) && !defined (AS_COMPILATION)
#error "Only <appstream-glib.h> can be included directly."
#endif

#ifndef __AS_APP_PRIVATE_H
#define __AS_APP_PRIVATE_H

#include <glib-object.h>

#include "as-app.h"
#include "as-node-private.h"
#include "as-stemmer.h"

G_BEGIN_DECLS

/**
 * AsAppProblems:
 * @AS_APP_PROBLEM_NONE:			No problems detected
 * @AS_APP_PROBLEM_NO_XML_HEADER:		No XML header found
 * @AS_APP_PROBLEM_NO_COPYRIGHT_INFO:		No copyright header found
 * @AS_APP_PROBLEM_DEPRECATED_LICENCE:		Deprecated <license> tag
 * @AS_APP_PROBLEM_MULTIPLE_ENTRIES:		Multiple <components> defined
 * @AS_APP_PROBLEM_INTLTOOL_NAME:		<_name> tag detected
 * @AS_APP_PROBLEM_INTLTOOL_SUMMARY:		<_summary> tag detected
 * @AS_APP_PROBLEM_INTLTOOL_DESCRIPTION:	<_p>, <_li> tag detected
 * @AS_APP_PROBLEM_NOT_VALID_UTF8:		Text with invalid UTF-8
 * @AS_APP_PROBLEM_PREFORMATTED_DESCRIPTION:	Preformatted description found
 * @AS_APP_PROBLEM_TRANSLATED_ID:		The <id> value was translated
 * @AS_APP_PROBLEM_TRANSLATED_LICENSE:		The <license> value was translated
 * @AS_APP_PROBLEM_TRANSLATED_PROJECT_GROUP:	The <project_group> value was translated
 * @AS_APP_PROBLEM_UPDATECONTACT_FALLBACK:	The file used <updatecontact> without a space
 * @AS_APP_PROBLEM_INVALID_PROJECT_GROUP:	Invalid project group detected
 * @AS_APP_PROBLEM_INVALID_XML_TAG:		Invalid XML tag name detected
 * @AS_APP_PROBLEM_EXPECTED_CHILDREN:		Children tags expected
 * @AS_APP_PROBLEM_INVALID_KEYWORDS:		One or more keywords was invalid
 * @AS_APP_PROBLEM_DUPLICATE_RELEASE:		More than one release with the same version
 * @AS_APP_PROBLEM_DUPLICATE_SCREENSHOT:	More than one screenshot with the same URL
 * @AS_APP_PROBLEM_DUPLICATE_CONTENT_RATING:	More than one content rating with the same kind
 * @AS_APP_PROBLEM_DUPLICATE_AGREEMENT:		More than one agreement with the same kind
 *
 * The application problems detected when loading.
 **/
typedef enum {
	AS_APP_PROBLEM_NONE			= 0,
	AS_APP_PROBLEM_NO_XML_HEADER		= 1 << 0,
	AS_APP_PROBLEM_NO_COPYRIGHT_INFO	= 1 << 1,
	AS_APP_PROBLEM_DEPRECATED_LICENCE	= 1 << 2,
	AS_APP_PROBLEM_MULTIPLE_ENTRIES		= 1 << 3,
	AS_APP_PROBLEM_INTLTOOL_NAME		= 1 << 4,
	AS_APP_PROBLEM_INTLTOOL_SUMMARY		= 1 << 5,
	AS_APP_PROBLEM_INTLTOOL_DESCRIPTION	= 1 << 6,
	AS_APP_PROBLEM_NOT_VALID_UTF8		= 1 << 7,
	AS_APP_PROBLEM_PREFORMATTED_DESCRIPTION	= 1 << 8,
	AS_APP_PROBLEM_TRANSLATED_ID		= 1 << 9,
	AS_APP_PROBLEM_TRANSLATED_LICENSE	= 1 << 10,
	AS_APP_PROBLEM_TRANSLATED_PROJECT_GROUP	= 1 << 11,
	AS_APP_PROBLEM_UPDATECONTACT_FALLBACK	= 1 << 12,
	AS_APP_PROBLEM_INVALID_PROJECT_GROUP	= 1 << 13,
	AS_APP_PROBLEM_INVALID_XML_TAG		= 1 << 14,
	AS_APP_PROBLEM_EXPECTED_CHILDREN	= 1 << 15,
	AS_APP_PROBLEM_INVALID_KEYWORDS		= 1 << 16,
	AS_APP_PROBLEM_DUPLICATE_RELEASE	= 1 << 17,
	AS_APP_PROBLEM_DUPLICATE_SCREENSHOT	= 1 << 18,
	AS_APP_PROBLEM_DUPLICATE_CONTENT_RATING	= 1 << 19,
	AS_APP_PROBLEM_DUPLICATE_AGREEMENT	= 1 << 20,
	/*< private >*/
	AS_APP_PROBLEM_LAST
} AsAppProblems;

/* some useful constants */
#define AS_APP_ICON_MIN_HEIGHT			32
#define AS_APP_ICON_MIN_WIDTH			32

/* unique */
#define AS_APP_UNIQUE_WILDCARD			"*"

AsAppProblems	 as_app_get_problems		(AsApp		*app);
guint		 as_app_get_name_size		(AsApp		*app);
guint		 as_app_get_comment_size	(AsApp		*app);
guint		 as_app_get_description_size	(AsApp		*app);
GPtrArray	*as_app_get_search_tokens	(AsApp		*app);
AsBundleKind	 as_app_get_bundle_kind		(AsApp		*app);

GNode		*as_app_node_insert		(AsApp		*app,
						 GNode		*parent,
						 AsNodeContext	*ctx);
gboolean	 as_app_node_parse		(AsApp		*app,
						 GNode		*node,
						 AsNodeContext	*ctx,
						 GError		**error);
gboolean	 as_app_node_parse_dep11	(AsApp		*app,
						 GNode		*node,
						 AsNodeContext	*ctx,
						 GError		**error);
gboolean	 as_app_parse_desktop_file	(AsApp		*app,
						 const gchar	*filename,
						 AsAppParseFlags flags,
						 GError		**error);
gboolean	 as_app_parse_inf_file		(AsApp		*app,
						 const gchar	*filename,
						 AsAppParseFlags flags,
						 GError		**error);
void		 as_app_set_stemmer		(AsApp		*app,
						 AsStemmer	*stemmer);
void		 as_app_set_search_blacklist	(AsApp		*app,
						 GHashTable	*search_blacklist);

G_END_DECLS

#endif /* __AS_APP_PRIVATE_H */
