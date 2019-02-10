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

G_BEGIN_DECLS

/**
 * AsInfError:
 * @AS_INF_ERROR_FAILED:			Generic failure
 * @AS_INF_ERROR_INVALID_TYPE:			Invalid type
 * @AS_INF_ERROR_NOT_FOUND:			Data not found
 *
 * The error type.
 **/
typedef enum {
	AS_INF_ERROR_FAILED,
	AS_INF_ERROR_INVALID_TYPE,
	AS_INF_ERROR_NOT_FOUND,
	/*< private >*/
	AS_INF_ERROR_LAST
} AsInfError;

#define	AS_INF_ERROR				as_inf_error_quark ()

/**
 * AsInfLoadFlags:
 * @AS_INF_LOAD_FLAG_NONE:			No flags set
 * @AS_INF_LOAD_FLAG_STRICT:			Be strict when loading the file
 * @AS_INF_LOAD_FLAG_CASE_INSENSITIVE:		Load keys and groups case insensitive
 *
 * The flags used when loading INF files.
 **/
typedef enum {
	AS_INF_LOAD_FLAG_NONE			= 0,
	AS_INF_LOAD_FLAG_STRICT			= 1 << 0,
	AS_INF_LOAD_FLAG_CASE_INSENSITIVE	= 1 << 1,
	/*< private >*/
	AS_INF_LOAD_FLAG_LAST
} AsInfLoadFlags;

GQuark		 as_inf_error_quark		(void);
gboolean	 as_inf_load_data		(GKeyFile	*keyfile,
						 const gchar	*data,
						 AsInfLoadFlags	 flags,
						 GError		**error)
G_DEPRECATED;
gboolean	 as_inf_load_file		(GKeyFile	*keyfile,
						 const gchar	*filename,
						 AsInfLoadFlags	 flags,
						 GError		**error)
G_DEPRECATED;
gchar		*as_inf_get_driver_version	(GKeyFile	*keyfile,
						 guint64	*timestamp,
						 GError		**error)
G_DEPRECATED;

G_END_DECLS
