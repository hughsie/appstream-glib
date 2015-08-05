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

#ifndef __AS_INF_H
#define __AS_INF_H

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
						 GError		**error);
gboolean	 as_inf_load_file		(GKeyFile	*keyfile,
						 const gchar	*filename,
						 AsInfLoadFlags	 flags,
						 GError		**error);
gchar		*as_inf_get_driver_version	(GKeyFile	*keyfile,
						 guint64	*timestamp,
						 GError		**error);

G_END_DECLS

#endif /* __AS_INF_H */
