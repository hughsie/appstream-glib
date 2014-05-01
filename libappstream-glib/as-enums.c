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
 * SECTION:as-enums
 * @short_description: Helper functions for converting to and from enum strings
 * @include: appstream-glib.h
 * @stability: Stable
 *
 * These helper functions may be useful if implementing an AppStream parser.
 */

#include "config.h"

#include "as-enums.h"

/**
 * as_id_kind_to_string:
 * @id_kind: the #AsIdKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @id_kind
 *
 * Since: 0.1.0
 **/
const gchar *
as_id_kind_to_string (AsIdKind id_kind)
{
	if (id_kind == AS_ID_KIND_DESKTOP)
		return "desktop";
	if (id_kind == AS_ID_KIND_CODEC)
		return "codec";
	if (id_kind == AS_ID_KIND_FONT)
		return "font";
	if (id_kind == AS_ID_KIND_INPUT_METHOD)
		return "inputmethod";
	if (id_kind == AS_ID_KIND_WEB_APP)
		return "webapp";
	if (id_kind == AS_ID_KIND_SOURCE)
		return "source";
	return "unknown";
}

/**
 * as_id_kind_from_string:
 * @id_kind: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AsIdKind or %AS_ID_KIND_UNKNOWN for unknown
 *
 * Since: 0.1.0
 **/
AsIdKind
as_id_kind_from_string (const gchar *id_kind)
{
	if (g_strcmp0 (id_kind, "desktop") == 0)
		return AS_ID_KIND_DESKTOP;
	if (g_strcmp0 (id_kind, "codec") == 0)
		return AS_ID_KIND_CODEC;
	if (g_strcmp0 (id_kind, "font") == 0)
		return AS_ID_KIND_FONT;
	if (g_strcmp0 (id_kind, "inputmethod") == 0)
		return AS_ID_KIND_INPUT_METHOD;
	if (g_strcmp0 (id_kind, "webapp") == 0)
		return AS_ID_KIND_WEB_APP;
	if (g_strcmp0 (id_kind, "source") == 0)
		return AS_ID_KIND_SOURCE;
	return AS_ID_KIND_UNKNOWN;
}

/**
 * as_icon_kind_to_string:
 * @icon_kind: the @AsIconKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @icon_kind
 *
 * Since: 0.1.0
 **/
const gchar *
as_icon_kind_to_string (AsIconKind icon_kind)
{
	if (icon_kind == AS_ICON_KIND_CACHED)
		return "cached";
	if (icon_kind == AS_ICON_KIND_STOCK)
		return "stock";
	if (icon_kind == AS_ICON_KIND_REMOTE)
		return "remote";
	return "unknown";
}

/**
 * as_icon_kind_from_string:
 * @icon_kind: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AsIconKind or %AS_ICON_KIND_UNKNOWN for unknown
 *
 * Since: 0.1.0
 **/
AsIconKind
as_icon_kind_from_string (const gchar *icon_kind)
{
	if (g_strcmp0 (icon_kind, "cached") == 0)
		return AS_ICON_KIND_CACHED;
	if (g_strcmp0 (icon_kind, "stock") == 0)
		return AS_ICON_KIND_STOCK;
	if (g_strcmp0 (icon_kind, "remote") == 0)
		return AS_ICON_KIND_REMOTE;
	return AS_ICON_KIND_UNKNOWN;
}

/**
 * as_url_kind_to_string:
 * @url_kind: the @AsUrlKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @url_kind
 *
 * Since: 0.1.0
 **/
const gchar *
as_url_kind_to_string (AsUrlKind url_kind)
{
	if (url_kind == AS_URL_KIND_HOMEPAGE)
		return "homepage";
	if (url_kind == AS_URL_KIND_BUGTRACKER)
		return "bugtracker";
	if (url_kind == AS_URL_KIND_FAQ)
		return "faq";
	if (url_kind == AS_URL_KIND_DONATION)
		return "donation";
	if (url_kind == AS_URL_KIND_HELP)
		return "help";
	return "unknown";
}

/**
 * as_url_kind_from_string:
 * @url_kind: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AsUrlKind or %AS_URL_KIND_UNKNOWN for unknown
 *
 * Since: 0.1.0
 **/
AsUrlKind
as_url_kind_from_string (const gchar *url_kind)
{
	if (g_strcmp0 (url_kind, "homepage") == 0)
		return AS_URL_KIND_HOMEPAGE;
	if (g_strcmp0 (url_kind, "bugtracker") == 0)
		return AS_URL_KIND_BUGTRACKER;
	if (g_strcmp0 (url_kind, "faq") == 0)
		return AS_URL_KIND_FAQ;
	if (g_strcmp0 (url_kind, "donation") == 0)
		return AS_URL_KIND_DONATION;
	if (g_strcmp0 (url_kind, "help") == 0)
		return AS_URL_KIND_HELP;
	return AS_URL_KIND_UNKNOWN;
}
