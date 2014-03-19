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
	GQuark qtag;
	guint i;
	static gboolean done_init = FALSE;
	static GQuark qenum[AS_TAG_LAST];

	if (!done_init) {
		for (i = 0; i < AS_TAG_LAST; i++)
			qenum[i] = g_quark_from_static_string (as_tag_to_string (i));
		done_init = TRUE;
	}
	qtag = g_quark_try_string (tag);
	for (i = 0; i < AS_TAG_LAST; i++)
		if (qenum[i] == qtag)
			return i;
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
	const gchar *str[] = {
		"unknown",
		"applications",
		"application",
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
		"updatecontact",
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
		NULL };
	if (tag > AS_TAG_LAST)
		tag = AS_TAG_LAST;
	return str[tag];
}
