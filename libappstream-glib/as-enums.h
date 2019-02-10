/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2016 Richard Hughes <richard@hughsie.com>
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
 * AsIdKind:
 * @AS_ID_KIND_UNKNOWN:			Type invalid or not known
 * @AS_ID_KIND_DESKTOP:			A desktop application
 * @AS_ID_KIND_FONT:			A font add-on
 * @AS_ID_KIND_CODEC:			A codec add-on
 * @AS_ID_KIND_INPUT_METHOD:		A input method add-on
 * @AS_ID_KIND_WEB_APP:			A web appication
 * @AS_ID_KIND_SOURCE:			A software source
 * @AS_ID_KIND_ADDON:			An addon, e.g. a plugin
 * @AS_ID_KIND_FIRMWARE:		A firmware update
 * @AS_ID_KIND_RUNTIME:			Runtime platform
 * @AS_ID_KIND_GENERIC:			Generic component
 *
 * The component type.
 **/
typedef enum {
	AS_ID_KIND_UNKNOWN,		/* Since: 0.1.0 */
	AS_ID_KIND_DESKTOP,		/* Since: 0.1.0 */
	AS_ID_KIND_FONT,		/* Since: 0.1.0 */
	AS_ID_KIND_CODEC,		/* Since: 0.1.0 */
	AS_ID_KIND_INPUT_METHOD,	/* Since: 0.1.0 */
	AS_ID_KIND_WEB_APP,		/* Since: 0.1.0 */
	AS_ID_KIND_SOURCE,		/* Since: 0.1.0 */
	AS_ID_KIND_ADDON,		/* Since: 0.1.7 */
	AS_ID_KIND_FIRMWARE,		/* Since: 0.3.5 */
	AS_ID_KIND_RUNTIME,		/* Since: 0.5.6 */
	AS_ID_KIND_GENERIC,		/* Since: 0.5.8 */
	/*< private >*/
	AS_ID_KIND_LAST
} AsIdKind G_GNUC_DEPRECATED_FOR(AsAppKind);

/**
 * AsUrlKind:
 * @AS_URL_KIND_UNKNOWN:		Type invalid or not known
 * @AS_URL_KIND_HOMEPAGE:		Application project homepage
 * @AS_URL_KIND_BUGTRACKER:		Application bugtracker
 * @AS_URL_KIND_FAQ:			Application FAQ page
 * @AS_URL_KIND_DONATION:		Application donation page
 * @AS_URL_KIND_HELP:			Application help manual
 * @AS_URL_KIND_MISSING:		The package is available, but missing
 * @AS_URL_KIND_TRANSLATE:		Application translation page
 * @AS_URL_KIND_DETAILS:		Release details
 * @AS_URL_KIND_SOURCE:			Link to source code
 * @AS_URL_KIND_CONTACT:		URL to contact developer on
 *
 * The URL type.
 **/
typedef enum {
	AS_URL_KIND_UNKNOWN,		/* Since: 0.1.0 */
	AS_URL_KIND_HOMEPAGE,		/* Since: 0.1.0 */
	AS_URL_KIND_BUGTRACKER,		/* Since: 0.1.1 */
	AS_URL_KIND_FAQ,		/* Since: 0.1.1 */
	AS_URL_KIND_DONATION,		/* Since: 0.1.1 */
	AS_URL_KIND_HELP,		/* Since: 0.1.5 */
	AS_URL_KIND_MISSING,		/* Since: 0.2.2 */
	AS_URL_KIND_TRANSLATE,		/* Since: 0.6.1 */
	AS_URL_KIND_DETAILS,		/* Since: 0.7.15 */
	AS_URL_KIND_SOURCE,		/* Since: 0.7.15 */
	AS_URL_KIND_CONTACT,		/* Since: 0.7.15 */
	/*< private >*/
	AS_URL_KIND_LAST
} AsUrlKind;

/**
 * AsKudoKind:
 * @AS_KUDO_KIND_UNKNOWN:		Type invalid or not known
 * @AS_KUDO_KIND_SEARCH_PROVIDER:	Installs a search provider
 * @AS_KUDO_KIND_USER_DOCS:		Installs user documentation
 * @AS_KUDO_KIND_APP_MENU:		Uses the GNOME application menu
 * @AS_KUDO_KIND_MODERN_TOOLKIT:	Uses a modern toolkit like GTK3 or QT5
 * @AS_KUDO_KIND_NOTIFICATIONS:		Registers notifications with KDE or GNOME
 * @AS_KUDO_KIND_HIGH_CONTRAST:		Installs a high contrast icon
 * @AS_KUDO_KIND_HI_DPI_ICON:		Installs a high DPI icon
 *
 * The kudo type.
 **/
typedef enum {
	AS_KUDO_KIND_UNKNOWN,		/* Since: 0.2.2 */
	AS_KUDO_KIND_SEARCH_PROVIDER,	/* Since: 0.2.2 */
	AS_KUDO_KIND_USER_DOCS, 	/* Since: 0.2.2 */
	AS_KUDO_KIND_APP_MENU,		/* Since: 0.2.2 */
	AS_KUDO_KIND_MODERN_TOOLKIT,	/* Since: 0.2.2 */
	AS_KUDO_KIND_NOTIFICATIONS,	/* Since: 0.2.2 */
	AS_KUDO_KIND_HIGH_CONTRAST,	/* Since: 0.3.0 */
	AS_KUDO_KIND_HI_DPI_ICON,	/* Since: 0.3.1 */
	/*< private >*/
	AS_KUDO_KIND_LAST
} AsKudoKind;

/**
 * AsUrgencyKind:
 * @AS_URGENCY_KIND_UNKNOWN:		Urgency invalid or not known
 * @AS_URGENCY_KIND_LOW:		Low urgency release
 * @AS_URGENCY_KIND_MEDIUM:		Medium urgency release
 * @AS_URGENCY_KIND_HIGH:		High urgency release
 * @AS_URGENCY_KIND_CRITICAL:		Critically urgent release
 *
 * The urgency of a release.
 **/
typedef enum {
	AS_URGENCY_KIND_UNKNOWN,	/* Since: 0.5.1 */
	AS_URGENCY_KIND_LOW,		/* Since: 0.5.1 */
	AS_URGENCY_KIND_MEDIUM,		/* Since: 0.5.1 */
	AS_URGENCY_KIND_HIGH,		/* Since: 0.5.1 */
	AS_URGENCY_KIND_CRITICAL,	/* Since: 0.5.1 */
	/*< private >*/
	AS_URGENCY_KIND_LAST
} AsUrgencyKind;

/**
 * AsSizeKind:
 * @AS_SIZE_KIND_UNKNOWN:		Not known
 * @AS_SIZE_KIND_INSTALLED:		Installed size
 * @AS_SIZE_KIND_DOWNLOAD:		Download size
 *
 * The release size kind.
 **/
typedef enum {
	AS_SIZE_KIND_UNKNOWN,		/* Since: 0.5.2 */
	AS_SIZE_KIND_INSTALLED,		/* Since: 0.5.2 */
	AS_SIZE_KIND_DOWNLOAD,		/* Since: 0.5.2 */
	/*< private >*/
	AS_SIZE_KIND_LAST
} AsSizeKind;

const gchar	*as_size_kind_to_string		(AsSizeKind	 size_kind);
AsSizeKind	 as_size_kind_from_string	(const gchar	*size_kind);

const gchar	*as_urgency_kind_to_string	(AsUrgencyKind	 urgency_kind);
AsUrgencyKind	 as_urgency_kind_from_string	(const gchar	*urgency_kind);

const gchar	*as_url_kind_to_string		(AsUrlKind	 url_kind);
AsUrlKind	 as_url_kind_from_string	(const gchar	*url_kind);

const gchar	*as_kudo_kind_to_string		(AsKudoKind	 kudo_kind);
AsKudoKind	 as_kudo_kind_from_string	(const gchar	*kudo_kind);

/* deprecated */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
const gchar	*as_id_kind_to_string		(AsIdKind	 id_kind)
G_DEPRECATED_FOR(as_app_kind_to_string);
AsIdKind	 as_id_kind_from_string		(const gchar	*id_kind)
G_DEPRECATED_FOR(as_app_kind_from_string);
G_GNUC_END_IGNORE_DEPRECATIONS

G_END_DECLS
