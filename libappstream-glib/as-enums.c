/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2016 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
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
#include "as-app.h"
#include "as-ref-string.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
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
	return as_app_kind_to_string (id_kind);
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
	return as_app_kind_from_string (id_kind);
}
G_GNUC_END_IGNORE_DEPRECATIONS

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
		return as_ref_string_new_static ("homepage");
	if (url_kind == AS_URL_KIND_BUGTRACKER)
		return as_ref_string_new_static ("bugtracker");
	if (url_kind == AS_URL_KIND_FAQ)
		return as_ref_string_new_static ("faq");
	if (url_kind == AS_URL_KIND_DONATION)
		return as_ref_string_new_static ("donation");
	if (url_kind == AS_URL_KIND_HELP)
		return as_ref_string_new_static ("help");
	if (url_kind == AS_URL_KIND_MISSING)
		return as_ref_string_new_static ("missing");
	if (url_kind == AS_URL_KIND_TRANSLATE)
		return as_ref_string_new_static ("translate");
	if (url_kind == AS_URL_KIND_DETAILS)
		return as_ref_string_new_static ("details");
	if (url_kind == AS_URL_KIND_SOURCE)
		return as_ref_string_new_static ("source");
	if (url_kind == AS_URL_KIND_CONTACT)
		return as_ref_string_new_static ("contact");
	return as_ref_string_new_static ("unknown");
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
	if (g_strcmp0 (url_kind, "translate") == 0)
		return AS_URL_KIND_TRANSLATE;
	if (g_strcmp0 (url_kind, "details") == 0)
		return AS_URL_KIND_DETAILS;
	if (g_strcmp0 (url_kind, "source") == 0)
		return AS_URL_KIND_SOURCE;
	if (g_strcmp0 (url_kind, "contact") == 0)
		return AS_URL_KIND_CONTACT;
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
		return as_ref_string_new_static ("SearchProvider");
	if (kudo_kind == AS_KUDO_KIND_USER_DOCS)
		return as_ref_string_new_static ("UserDocs");
	if (kudo_kind == AS_KUDO_KIND_APP_MENU)
		return as_ref_string_new_static ("AppMenu");
	if (kudo_kind == AS_KUDO_KIND_MODERN_TOOLKIT)
		return as_ref_string_new_static ("ModernToolkit");
	if (kudo_kind == AS_KUDO_KIND_NOTIFICATIONS)
		return as_ref_string_new_static ("Notifications");
	if (kudo_kind == AS_KUDO_KIND_HIGH_CONTRAST)
		return as_ref_string_new_static ("HighContrast");
	if (kudo_kind == AS_KUDO_KIND_HI_DPI_ICON)
		return as_ref_string_new_static ("HiDpiIcon");
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
 * as_size_kind_to_string:
 * @size_kind: the #AsSizeKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @size_kind
 *
 * Since: 0.5.2
 **/
const gchar *
as_size_kind_to_string (AsSizeKind size_kind)
{
	if (size_kind == AS_SIZE_KIND_INSTALLED)
		return as_ref_string_new_static ("installed");
	if (size_kind == AS_SIZE_KIND_DOWNLOAD)
		return as_ref_string_new_static ("download");
	return as_ref_string_new_static ("unknown");
}

/**
 * as_size_kind_from_string:
 * @size_kind: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AsSizeKind or %AS_SIZE_KIND_UNKNOWN for unknown
 *
 * Since: 0.5.2
 **/
AsSizeKind
as_size_kind_from_string (const gchar *size_kind)
{
	if (g_strcmp0 (size_kind, "installed") == 0)
		return AS_SIZE_KIND_INSTALLED;
	if (g_strcmp0 (size_kind, "download") == 0)
		return AS_SIZE_KIND_DOWNLOAD;
	return AS_SIZE_KIND_UNKNOWN;
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
		return as_ref_string_new_static ("low");
	if (urgency_kind == AS_URGENCY_KIND_MEDIUM)
		return as_ref_string_new_static ("medium");
	if (urgency_kind == AS_URGENCY_KIND_HIGH)
		return as_ref_string_new_static ("high");
	if (urgency_kind == AS_URGENCY_KIND_CRITICAL)
		return as_ref_string_new_static ("critical");
	return as_ref_string_new_static ("unknown");
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
