/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#if !defined (__APPSTREAM_GLIB_H_INSIDE__) && !defined (AS_COMPILATION)
#error "Only <appstream-glib.h> can be included directly."
#endif

#include <glib.h>

#include "as-app.h"
#include "as-bundle.h"

G_BEGIN_DECLS

/**
 * AsUtilsError:
 * @AS_UTILS_ERROR_FAILED:			Generic failure
 * @AS_UTILS_ERROR_INVALID_TYPE:		Invalid type
 *
 * The error type.
 **/
typedef enum {
	AS_UTILS_ERROR_FAILED,
	AS_UTILS_ERROR_INVALID_TYPE,
	/*< private >*/
	AS_UTILS_ERROR_LAST
} AsUtilsError;

#define	AS_UTILS_ERROR				as_utils_error_quark ()

/**
 * AsUtilsFindIconFlag:
 * @AS_UTILS_FIND_ICON_NONE:			No flags set
 * @AS_UTILS_FIND_ICON_HI_DPI:			Prefer a HiDPI icon
 *
 * The flags used when finding icons.
 **/
typedef enum {
	AS_UTILS_FIND_ICON_NONE			= 0,
	AS_UTILS_FIND_ICON_HI_DPI		= 1 << 0,
	/*< private >*/
	AS_UTILS_FIND_ICON_LAST
} AsUtilsFindIconFlag;

/**
 * AsUtilsLocation:
 * @AS_UTILS_LOCATION_SHARED:			Installed by the vendor, shared
 * @AS_UTILS_LOCATION_CACHE:			Installed as metadata, shared
 * @AS_UTILS_LOCATION_USER:			Installed by the user
 *
 * The flags used when installing and removing metadata files.
 **/
typedef enum {
	AS_UTILS_LOCATION_SHARED,
	AS_UTILS_LOCATION_CACHE,
	AS_UTILS_LOCATION_USER,
	/*< private >*/
	AS_UTILS_LOCATION_LAST
} AsUtilsLocation;

/**
 * AsVersionParseFlag:
 * @AS_VERSION_PARSE_FLAG_NONE:			No flags set
 * @AS_VERSION_PARSE_FLAG_USE_TRIPLET:		Use Microsoft-style version numbers
 * @AS_VERSION_PARSE_FLAG_USE_BCD:		Use binary coded decimal notation
 *
 * The flags used when parsing version numbers.
 **/
typedef enum {
	AS_VERSION_PARSE_FLAG_NONE		= 0,
	AS_VERSION_PARSE_FLAG_USE_TRIPLET	= 1 << 0,
	AS_VERSION_PARSE_FLAG_USE_BCD		= 1 << 1,	/* Since: 0.7.3 */
	/*< private >*/
	AS_VERSION_PARSE_FLAG_LAST
} AsVersionParseFlag;

/**
 * AsVersionCompareFlag:
 * @AS_VERSION_COMPARE_FLAG_NONE:		No flags set
 * @AS_VERSION_COMPARE_FLAG_USE_HEURISTICS:	Use a heuristic to parse version numbers
 *
 * The flags used when comparing version numbers.
 **/
typedef enum {
	AS_VERSION_COMPARE_FLAG_NONE		= 0,
	AS_VERSION_COMPARE_FLAG_USE_HEURISTICS	= 1 << 0,
	/*< private >*/
	AS_VERSION_COMPARE_FLAG_LAST
} AsVersionCompareFlag;

/**
 * AsUniqueIdMatchFlags:
 * @AS_UNIQUE_ID_MATCH_FLAG_NONE:		No flags set
 * @AS_UNIQUE_ID_MATCH_FLAG_SCOPE:		Scope, e.g. a #AsAppScope
 * @AS_UNIQUE_ID_MATCH_FLAG_BUNDLE_KIND:	Bundle kind, e.g. a #AsBundleKind
 * @AS_UNIQUE_ID_MATCH_FLAG_ORIGIN:		Origin
 * @AS_UNIQUE_ID_MATCH_FLAG_KIND:		Component kind, e.g. a #AsAppKind
 * @AS_UNIQUE_ID_MATCH_FLAG_ID:			Component AppStream ID
 * @AS_UNIQUE_ID_MATCH_FLAG_BRANCH:		Branch
 *
 * The flags used when matching unique IDs.
 **/
typedef enum {
	AS_UNIQUE_ID_MATCH_FLAG_NONE		= 0,
	AS_UNIQUE_ID_MATCH_FLAG_SCOPE		= 1 << 0,
	AS_UNIQUE_ID_MATCH_FLAG_BUNDLE_KIND	= 1 << 1,	/* Since: 0.7.8 */
	AS_UNIQUE_ID_MATCH_FLAG_ORIGIN		= 1 << 2,	/* Since: 0.7.8 */
	AS_UNIQUE_ID_MATCH_FLAG_KIND		= 1 << 3,	/* Since: 0.7.8 */
	AS_UNIQUE_ID_MATCH_FLAG_ID		= 1 << 4,	/* Since: 0.7.8 */
	AS_UNIQUE_ID_MATCH_FLAG_BRANCH		= 1 << 5,	/* Since: 0.7.8 */
	/*< private >*/
	AS_UNIQUE_ID_MATCH_FLAG_LAST
} AsUniqueIdMatchFlags;

GQuark		 as_utils_error_quark		(void);
gboolean	 as_utils_is_stock_icon_name	(const gchar	*name);
gboolean	 as_utils_is_spdx_license_id	(const gchar	*license_id);
gboolean	 as_utils_is_spdx_license	(const gchar	*license);
gboolean	 as_utils_is_environment_id	(const gchar	*environment_id);
gboolean	 as_utils_is_category_id	(const gchar	*category_id);

G_DEPRECATED
gboolean	 as_utils_is_blacklisted_id	(const gchar	*desktop_id);

gchar		**as_utils_spdx_license_tokenize (const gchar	*license);
gchar		*as_utils_spdx_license_detokenize (gchar	**license_tokens);
gchar		*as_utils_license_to_spdx	(const gchar	*license);
gchar		*as_utils_find_icon_filename	(const gchar	*destdir,
						 const gchar	*search,
						 GError		**error);
gchar		*as_utils_find_icon_filename_full (const gchar	*destdir,
						 const gchar	*search,
						 AsUtilsFindIconFlag flags,
						 GError		**error);
gboolean	 as_utils_install_filename	(AsUtilsLocation location,
						 const gchar	*filename,
						 const gchar	*origin,
						 const gchar	*destdir,
						 GError		**error);
gboolean	 as_utils_search_token_valid	(const gchar	*token);
gchar		**as_utils_search_tokenize	(const gchar	*search);
gint		 as_utils_vercmp_full		(const gchar	*version_a,
						 const gchar	*version_b,
						 AsVersionCompareFlag flags);
gint		 as_utils_vercmp		(const gchar	*version_a,
						 const gchar	*version_b);
gboolean	 as_utils_guid_is_valid		(const gchar	*guid);
gchar		*as_utils_guid_from_string	(const gchar	*str);
gchar		*as_utils_guid_from_data	(const gchar	*namespace_id,
						 const guint8	*data,
						 gsize		 data_len,
						 GError		**error);
gchar		*as_utils_version_from_uint32	(guint32	 val,
						 AsVersionParseFlag flags);
gchar		*as_utils_version_from_uint16	(guint16	 val,
						 AsVersionParseFlag flags);
gchar		*as_utils_version_parse		(const gchar	*version);
guint		 as_utils_string_replace	(GString	*string,
						 const gchar	*search,
						 const gchar	*replace);
gchar		*as_utils_unique_id_build	(AsAppScope	 scope,
						 AsBundleKind	 bundle_kind,
						 const gchar	*origin,
						 AsAppKind	 kind,
						 const gchar	*id,
						 const gchar	*branch);
gboolean	 as_utils_unique_id_equal	(const gchar	*unique_id1,
						 const gchar	*unique_id2);
gboolean	 as_utils_unique_id_match	(const gchar	*unique_id1,
						 const gchar	*unique_id2,
						 AsUniqueIdMatchFlags match_flags);
gboolean	 as_utils_unique_id_valid	(const gchar	*unique_id);
guint		 as_utils_unique_id_hash	(const gchar	*unique_id);
gchar		*as_utils_appstream_id_build	(const gchar	*str);
gboolean	 as_utils_appstream_id_valid	(const gchar	*str);

G_END_DECLS
