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

/**
 * SECTION:as-tag
 * @short_description: Helper functions to convert to and from tag enums
 * @include: appstream-glib.h
 * @stability: Stable
 *
 * These functions will convert a tag enum such as %AS_TAG_APPLICATION to
 * it's string form, and also vice-versa.
 *
 * These helper functions may be useful if implementing an AppStream parser.
 */

#include "config.h"

#include "as-tag.h"

/**
 * as_tag_from_string:
 * @tag: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a %AsTag, or %AS_TAG_UNKNOWN if not known.
 *
 * Since: 0.1.0
 **/
AsTag
as_tag_from_string (const gchar *tag)
{
	if (g_strcmp0 (tag, "applications") == 0)
		return AS_TAG_APPLICATIONS;
	if (g_strcmp0 (tag, "application") == 0)
		return AS_TAG_APPLICATION;
	if (g_strcmp0 (tag, "id") == 0)
		return AS_TAG_ID;
	if (g_strcmp0 (tag, "pkgname") == 0)
		return AS_TAG_PKGNAME;
	if (g_strcmp0 (tag, "name") == 0)
		return AS_TAG_NAME;
	if (g_strcmp0 (tag, "summary") == 0)
		return AS_TAG_SUMMARY;
	if (g_strcmp0 (tag, "project_group") == 0)
		return AS_TAG_PROJECT_GROUP;
	if (g_strcmp0 (tag, "url") == 0)
		return AS_TAG_URL;
	if (g_strcmp0 (tag, "description") == 0)
		return AS_TAG_DESCRIPTION;
	if (g_strcmp0 (tag, "icon") == 0)
		return AS_TAG_ICON;
	if (g_strcmp0 (tag, "appcategories") == 0) /* deprecated */
		return AS_TAG_CATEGORIES;
	if (g_strcmp0 (tag, "categories") == 0)
		return AS_TAG_CATEGORIES;
	if (g_strcmp0 (tag, "appcategory") == 0) /* deprecated */
		return AS_TAG_CATEGORY;
	if (g_strcmp0 (tag, "category") == 0)
		return AS_TAG_CATEGORY;
	if (g_strcmp0 (tag, "keywords") == 0)
		return AS_TAG_KEYWORDS;
	if (g_strcmp0 (tag, "keyword") == 0)
		return AS_TAG_KEYWORD;
	if (g_strcmp0 (tag, "mimetypes") == 0)
		return AS_TAG_MIMETYPES;
	if (g_strcmp0 (tag, "mimetype") == 0)
		return AS_TAG_MIMETYPE;
	if (g_strcmp0 (tag, "project_license") == 0)
		return AS_TAG_PROJECT_LICENSE;
	if (g_strcmp0 (tag, "licence") == 0) /* deprecated */
		return AS_TAG_PROJECT_LICENSE;
	if (g_strcmp0 (tag, "screenshots") == 0)
		return AS_TAG_SCREENSHOTS;
	if (g_strcmp0 (tag, "screenshot") == 0)
		return AS_TAG_SCREENSHOT;
	if (g_strcmp0 (tag, "updatecontact") == 0)
		return AS_TAG_UPDATE_CONTACT;
	if (g_strcmp0 (tag, "image") == 0)
		return AS_TAG_IMAGE;
	if (g_strcmp0 (tag, "compulsory_for_desktop") == 0)
		return AS_TAG_COMPULSORY_FOR_DESKTOP;
	if (g_strcmp0 (tag, "priority") == 0)
		return AS_TAG_PRIORITY;
	if (g_strcmp0 (tag, "caption") == 0)
		return AS_TAG_CAPTION;
	if (g_strcmp0 (tag, "languages") == 0)
		return AS_TAG_LANGUAGES;
	if (g_strcmp0 (tag, "lang") == 0)
		return AS_TAG_LANG;
	if (g_strcmp0 (tag, "metadata") == 0)
		return AS_TAG_METADATA;
	if (g_strcmp0 (tag, "value") == 0)
		return AS_TAG_VALUE;
	if (g_strcmp0 (tag, "releases") == 0)
		return AS_TAG_RELEASES;
	if (g_strcmp0 (tag, "release") == 0)
		return AS_TAG_RELEASE;
	return AS_TAG_UNKNOWN;
}

/**
 * as_tag_to_string:
 * @tag: the %AsTag value.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @tag
 *
 * Since: 0.1.0
 **/
const gchar *
as_tag_to_string (AsTag tag)
{
	if (tag == AS_TAG_APPLICATIONS)
		return "applications";
	if (tag == AS_TAG_APPLICATION)
		return "application";
	if (tag == AS_TAG_ID)
		return "id";
	if (tag == AS_TAG_PKGNAME)
		return "pkgname";
	if (tag == AS_TAG_NAME)
		return "name";
	if (tag == AS_TAG_SUMMARY)
		return "summary";
	if (tag == AS_TAG_PROJECT_GROUP)
		return "project_group";
	if (tag == AS_TAG_URL)
		return "url";
	if (tag == AS_TAG_DESCRIPTION)
		return "description";
	if (tag == AS_TAG_ICON)
		return "icon";
	if (tag == AS_TAG_CATEGORIES)
		return "categories";
	if (tag == AS_TAG_CATEGORY)
		return "category";
	if (tag == AS_TAG_KEYWORDS)
		return "keywords";
	if (tag == AS_TAG_KEYWORD)
		return "keyword";
	if (tag == AS_TAG_MIMETYPES)
		return "mimetypes";
	if (tag == AS_TAG_MIMETYPE)
		return "mimetype";
	if (tag == AS_TAG_PROJECT_LICENSE)
		return "project_license";
	if (tag == AS_TAG_SCREENSHOTS)
		return "screenshots";
	if (tag == AS_TAG_SCREENSHOT)
		return "screenshot";
	if (tag == AS_TAG_UPDATE_CONTACT)
		return "updatecontact";
	if (tag == AS_TAG_IMAGE)
		return "image";
	if (tag == AS_TAG_COMPULSORY_FOR_DESKTOP)
		return "compulsory_for_desktop";
	if (tag == AS_TAG_PRIORITY)
		return "priority";
	if (tag == AS_TAG_CAPTION)
		return "caption";
	if (tag == AS_TAG_LANGUAGES)
		return "languages";
	if (tag == AS_TAG_LANG)
		return "lang";
	if (tag == AS_TAG_METADATA)
		return "metadata";
	if (tag == AS_TAG_VALUE)
		return "value";
	if (tag == AS_TAG_RELEASES)
		return "releases";
	if (tag == AS_TAG_RELEASE)
		return "release";
	return NULL;
}
