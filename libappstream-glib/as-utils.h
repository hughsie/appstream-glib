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

#if !defined (__APPSTREAM_GLIB_H) && !defined (AS_COMPILATION)
#error "Only <appstream-glib.h> can be included directly."
#endif

#ifndef __AS_UTILS_H
#define __AS_UTILS_H

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
 *
 * The flags used when parsing version numbers.
 **/
typedef enum {
	AS_VERSION_PARSE_FLAG_NONE		= 0,
	AS_VERSION_PARSE_FLAG_USE_TRIPLET	= 1 << 0,
	/*< private >*/
	AS_VERSION_PARSE_FLAG_LAST
} AsVersionParseFlag;

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
gboolean	 as_utils_unique_id_valid	(const gchar	*unique_id);
guint		 as_utils_unique_id_hash	(const gchar	*unique_id);
gchar		*as_utils_appstream_id_build	(const gchar	*str);
gboolean	 as_utils_appstream_id_valid	(const gchar	*str);

G_END_DECLS

#endif /* __AS_UTILS_H */
