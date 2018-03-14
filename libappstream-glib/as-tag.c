/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2017 Richard Hughes <richard@hughsie.com>
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
 * These functions will convert a tag enum such as %AS_TAG_COMPONENT to
 * it's string form, and also vice-versa.
 *
 * These helper functions may be useful if implementing an AppStream parser.
 */

#include "config.h"

#include "as-tag.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#include "as-tag-private.h"
#pragma GCC diagnostic pop

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
	return as_tag_from_string_full (tag, AS_TAG_FLAG_NONE);
}

/**
 * as_tag_from_string_full:
 * @tag: the string.
 * @flags: the #AsTagFlags e.g. %AS_TAG_FLAG_USE_FALLBACKS
 *
 * Converts the text representation to an enumerated value also converting
 * legacy key names.
 *
 * Returns: a %AsTag, or %AS_TAG_UNKNOWN if not known.
 *
 * Since: 0.1.2
 **/
AsTag
as_tag_from_string_full (const gchar *tag, AsTagFlags flags)
{
	const struct tag_data *ky;
	AsTag etag = AS_TAG_UNKNOWN;

	/* invalid */
	if (tag == NULL)
		return AS_TAG_UNKNOWN;

	/* use a perfect hash */
	ky = _as_tag_from_gperf (tag, strlen (tag));
	if (ky != NULL)
		etag = ky->etag;

	/* deprecated names */
	if (etag == AS_TAG_UNKNOWN && (flags & AS_TAG_FLAG_USE_FALLBACKS)) {
		if (g_strcmp0 (tag, "appcategories") == 0)
			return AS_TAG_CATEGORIES;
		if (g_strcmp0 (tag, "appcategory") == 0)
			return AS_TAG_CATEGORY;
		if (g_strcmp0 (tag, "licence") == 0)
			return AS_TAG_PROJECT_LICENSE;
		if (g_strcmp0 (tag, "applications") == 0)
			return AS_TAG_COMPONENTS;
		if (g_strcmp0 (tag, "application") == 0)
			return AS_TAG_COMPONENT;
		if (g_strcmp0 (tag, "updatecontact") == 0)
			return AS_TAG_UPDATE_CONTACT;
		/* fix spelling error */
		if (g_strcmp0 (tag, "metadata_licence") == 0)
			return AS_TAG_METADATA_LICENSE;
	}

	/* translated versions */
	if (etag == AS_TAG_UNKNOWN && (flags & AS_TAG_FLAG_USE_TRANSLATED)) {
		if (g_strcmp0 (tag, "_name") == 0)
			return AS_TAG_NAME;
		if (g_strcmp0 (tag, "_summary") == 0)
			return AS_TAG_SUMMARY;
		if (g_strcmp0 (tag, "_caption") == 0)
			return AS_TAG_CAPTION;
	}

	return etag;
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
	const gchar *str[] = {
		"unknown",
		"components",
		"component",
		"id",
		"pkgname",
		"name",
		"summary",
		"description",
		"url",
		"icon",
		"categories",
		"category",
		"keywords",
		"keyword",
		"mimetypes",
		"mimetype",
		"project_group",
		"project_license",
		"screenshot",
		"screenshots",
		"update_contact",
		"image",
		"compulsory_for_desktop",
		"priority",
		"caption",
		"languages",
		"lang",
		"metadata",
		"value",
		"releases",
		"release",
		"architectures",
		"arch",
		"metadata_license",
		"provides",
		"extends",
		"developer_name",
		"kudos",
		"kudo",
		"source_pkgname",
		"vetos",
		"veto",
		"bundle",
		"permissions",
		"permission",
		"location",
		"checksum",
		"size",
		"translation",
		"content_rating",
		"content_attribute",
		"version",
		"reviews",
		"review",
		"reviewer_name",
		"reviewer_id",
		"suggests",
		"requires",
		"custom",
		"launchable",
		"agreement",
		"agreement_section",
		NULL };
	if (tag > AS_TAG_LAST)
		tag = AS_TAG_LAST;
	return str[tag];
}
