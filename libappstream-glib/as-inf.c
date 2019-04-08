/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

/**
 * SECTION:as-inf
 * @short_description: Helper functions for parsing .inf files
 * @include: appstream-glib.h
 * @stability: Stable
 *
 * These functions are used internally to libappstream-glib, and some may be
 * useful to user-applications.
 */

#include "config.h"

#include <gio/gio.h>
//#include <string.h>
//#include <stdlib.h>

#include "as-inf.h"

/**
 * as_inf_error_quark:
 *
 * Return value: An error quark.
 *
 * Since: 0.3.7
 **/
G_DEFINE_QUARK (as-inf-error-quark, as_inf_error)

/**
 * as_inf_load_data:
 * @keyfile: a #GKeyFile
 * @data: the .inf file date to parse
 * @flags: #AsInfLoadFlags, e.g. %AS_INF_LOAD_FLAG_NONE
 * @error: A #GError or %NULL
 *
 * Repairs .inf file data and opens it as a keyfile.
 *
 * Important: The group and keynames are all forced to lower case as INF files
 * are specified as case insensitive and GKeyFile *is* case sensitive.
 * Any backslashes or spaces in the key name are also converted to '_'.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.3.5
 */
gboolean
as_inf_load_data (GKeyFile *keyfile,
		  const gchar *data,
		  AsInfLoadFlags flags,
		  GError **error)
{
	g_set_error (error,
		     AS_INF_ERROR,
		     AS_INF_ERROR_FAILED,
		     "Loading .inf data is no longer supported, see libginf");
	return FALSE;
}

/**
 * as_inf_load_file:
 * @keyfile: a #GKeyFile
 * @filename: the .inf file to open
 * @flags: #AsInfLoadFlags, e.g. %AS_INF_LOAD_FLAG_NONE
 * @error: A #GError or %NULL
 *
 * Repairs an .inf file and opens it as a keyfile.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.3.5
 */
gboolean
as_inf_load_file (GKeyFile *keyfile,
		  const gchar *filename,
		  AsInfLoadFlags flags,
		  GError **error)
{
	g_set_error (error,
		     AS_INF_ERROR,
		     AS_INF_ERROR_FAILED,
		     "Loading .inf files is no longer supported, see libginf");
	return FALSE;
}

/**
 * as_inf_get_driver_version:
 * @keyfile: a #GKeyFile
 * @timestamp: the returned driverver timestamp, or %NULL
 * @error: A #GError or %NULL
 *
 * Parses the DriverVer string into a recognisable version and timestamp;
 *
 * Returns: the version string, or %NULL for error.
 *
 * Since: 0.3.5
 */
gchar *
as_inf_get_driver_version (GKeyFile *keyfile, guint64 *timestamp, GError **error)
{
	g_set_error (error,
		     AS_INF_ERROR,
		     AS_INF_ERROR_FAILED,
		     "Loading .inf files is no longer supported, see libginf");
	return NULL;
}
