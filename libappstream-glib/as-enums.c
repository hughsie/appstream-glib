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
	if (id_kind == AS_ID_KIND_ADDON)
		return "addon";
	if (id_kind == AS_ID_KIND_FIRMWARE)
		return "firmware";
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
	if (g_strcmp0 (id_kind, "addon") == 0)
		return AS_ID_KIND_ADDON;
	if (g_strcmp0 (id_kind, "firmware") == 0)
		return AS_ID_KIND_FIRMWARE;
	return AS_ID_KIND_UNKNOWN;
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
	if (url_kind == AS_URL_KIND_MISSING)
		return "missing";
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
	if (g_strcmp0 (url_kind, "missing") == 0)
		return AS_URL_KIND_MISSING;
	return AS_URL_KIND_UNKNOWN;
}

/**
 * as_kudo_kind_to_string:
 * @kudo_kind: the @AsKudoKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @kudo_kind
 *
 * Since: 0.2.2
 **/
const gchar *
as_kudo_kind_to_string (AsKudoKind kudo_kind)
{
	if (kudo_kind == AS_KUDO_KIND_SEARCH_PROVIDER)
		return "SearchProvider";
	if (kudo_kind == AS_KUDO_KIND_USER_DOCS)
		return "UserDocs";
	if (kudo_kind == AS_KUDO_KIND_APP_MENU)
		return "AppMenu";
	if (kudo_kind == AS_KUDO_KIND_MODERN_TOOLKIT)
		return "ModernToolkit";
	if (kudo_kind == AS_KUDO_KIND_NOTIFICATIONS)
		return "Notifications";
	if (kudo_kind == AS_KUDO_KIND_HIGH_CONTRAST)
		return "HighContrast";
	if (kudo_kind == AS_KUDO_KIND_HI_DPI_ICON)
		return "HiDpiIcon";
	return NULL;
}

/**
 * as_kudo_kind_from_string:
 * @kudo_kind: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AsKudoKind or %AS_KUDO_KIND_UNKNOWN for unknown
 *
 * Since: 0.2.2
 **/
AsKudoKind
as_kudo_kind_from_string (const gchar *kudo_kind)
{
	if (g_strcmp0 (kudo_kind, "SearchProvider") == 0)
		return AS_KUDO_KIND_SEARCH_PROVIDER;
	if (g_strcmp0 (kudo_kind, "UserDocs") == 0)
		return AS_KUDO_KIND_USER_DOCS;
	if (g_strcmp0 (kudo_kind, "AppMenu") == 0)
		return AS_KUDO_KIND_APP_MENU;
	if (g_strcmp0 (kudo_kind, "ModernToolkit") == 0)
		return AS_KUDO_KIND_MODERN_TOOLKIT;
	if (g_strcmp0 (kudo_kind, "Notifications") == 0)
		return AS_KUDO_KIND_NOTIFICATIONS;
	if (g_strcmp0 (kudo_kind, "HighContrast") == 0)
		return AS_KUDO_KIND_HIGH_CONTRAST;
	if (g_strcmp0 (kudo_kind, "HiDpiIcon") == 0)
		return AS_KUDO_KIND_HI_DPI_ICON;
	return AS_KUDO_KIND_UNKNOWN;
}

/**
 * as_urgency_kind_to_string:
 * @urgency_kind: the #AsUrgencyKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @urgency_kind
 *
 * Since: 0.5.1
 **/
const gchar *
as_urgency_kind_to_string (AsUrgencyKind urgency_kind)
{
	if (urgency_kind == AS_URGENCY_KIND_LOW)
		return "low";
	if (urgency_kind == AS_URGENCY_KIND_MEDIUM)
		return "medium";
	if (urgency_kind == AS_URGENCY_KIND_HIGH)
		return "high";
	if (urgency_kind == AS_URGENCY_KIND_CRITICAL)
		return "critical";
	return "unknown";
}

/**
 * as_urgency_kind_from_string:
 * @urgency_kind: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AsUrgencyKind or %AS_URGENCY_KIND_UNKNOWN for unknown
 *
 * Since: 0.5.1
 **/
AsUrgencyKind
as_urgency_kind_from_string (const gchar *urgency_kind)
{
	if (g_strcmp0 (urgency_kind, "low") == 0)
		return AS_URGENCY_KIND_LOW;
	if (g_strcmp0 (urgency_kind, "medium") == 0)
		return AS_URGENCY_KIND_MEDIUM;
	if (g_strcmp0 (urgency_kind, "high") == 0)
		return AS_URGENCY_KIND_HIGH;
	if (g_strcmp0 (urgency_kind, "critical") == 0)
		return AS_URGENCY_KIND_CRITICAL;
	return AS_URGENCY_KIND_UNKNOWN;
}
